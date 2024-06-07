/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "Session.h"

#include <IGLU/simple_renderer/Drawable.h>
#include <IGLU/simple_renderer/Material.h>
#include <igl/ShaderCreator.h>

namespace iglu {
namespace imgui {

/* internal renderer -- based on imgui_impl_metal.mm */

#define PLAIN_SHADER_STRINGIFY(...) #__VA_ARGS__
#define PLAIN_SHADER(...) PLAIN_SHADER_STRINGIFY(__VA_ARGS__)

static const char* metalShaderStr() {
  return PLAIN_SHADER(
      using namespace metal;

      struct Uniforms { float4x4 projectionMatrix; };

      struct VertexIn {
        float2 position [[attribute(0)]];
        float2 texCoords [[attribute(1)]];
        float4 color [[attribute(2)]];
      };

      struct VertexOut {
        float4 position [[position]];
        float2 texCoords;
        float4 color;
      };

      vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                                   constant Uniforms & uniforms [[buffer(1)]]) {
        VertexOut out;
        out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);
        out.texCoords = in.texCoords;
        out.color = in.color;
        return out;
      }

      fragment half4 fragment_main(VertexOut in [[stage_in]],
                                   texture2d<half, access::sample> texture [[texture(0)]]) {
        constexpr sampler linearSampler(
            coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);
        half4 texColor = texture.sample(linearSampler, in.texCoords);
        return half4(in.color) * texColor;
      }

  );
}

static std::string getOpenGLVertexShaderSource(igl::ShaderVersion shaderVersion) {
  std::string shader;
  if (shaderVersion.majorVersion > 1 || shaderVersion.minorVersion > 30 ||
      shaderVersion.family == igl::ShaderFamily::GlslEs) {
#if IGL_PLATFORM_MACOS
    shader += "#version 100\n";
#endif
    shader += "precision mediump float;";
  }
  shader += PLAIN_SHADER(attribute vec2 position; attribute vec2 texCoords; attribute vec4 color;
                         uniform mat4 projectionMatrix;
                         varying vec2 Frag_UV;
                         varying vec4 Frag_Color;
                         void main() {
                           Frag_UV = texCoords;
                           Frag_Color = color;
                           gl_Position = projectionMatrix * vec4(position.xy, 0, 1);
                         });

  return shader;
}
static const char* getVulkanVertexShaderSource() {
  return R"(
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec4 col;

layout (location = 0) out vec4 color;
layout (location = 1) out vec2 uv;

layout(push_constant) uniform PushConstants {
    mat4 proj;
} pc;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    color = col;
    uv = texCoords;
    gl_Position = pc.proj * vec4(position.xy, 0, 1);
})";
}

static std::string getOpenGLFragmentShaderSource(igl::ShaderVersion shaderVersion) {
  std::string shader;
  if (shaderVersion.majorVersion > 1 || shaderVersion.minorVersion > 30 ||
      shaderVersion.family == igl::ShaderFamily::GlslEs) {
#if IGL_PLATFORM_MACOS
    shader += "#version 100\n";
#endif
    shader += "precision mediump float;";
  }
  shader +=
      PLAIN_SHADER(uniform sampler2D texture; varying vec2 Frag_UV; varying vec4 Frag_Color;
                   void main() { gl_FragColor = Frag_Color * texture2D(texture, Frag_UV.st); });
  return shader;
}
static const char* getVulkanFragmentShaderSource() {
  return R"(
layout(location = 0) out vec4 fColor;
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;

layout (set = 0, binding = 0) uniform sampler2D uTex;

void main() {
  fColor = color * texture(uTex, uv);
})";
}

static std::unique_ptr<igl::IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  igl::Result result;
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan: {
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource(),
                                                           "main",
                                                           "Shader Module: imgui::vertex",
                                                           getVulkanFragmentShaderSource(),
                                                           "main",
                                                           "Shader Module: imgui::fragment",
                                                           &result);
    break;
  }
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Metal: {
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, metalShaderStr(), "vertex_main", "fragment_main", "", &result);
    break;
  }
  case igl::BackendType::OpenGL: {
    auto shaderVersion = device.getShaderVersion();
    std::string vertexStr = getOpenGLVertexShaderSource(shaderVersion);
    std::string fragmentStr = getOpenGLFragmentShaderSource(shaderVersion);
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, vertexStr.c_str(), "main", "", fragmentStr.c_str(), "main", "", &result);
    break;
  }
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

struct DrawableData {
  std::shared_ptr<iglu::vertexdata::VertexData> vertexData;
  std::shared_ptr<iglu::drawable::Drawable> drawable;

  DrawableData(igl::IDevice& device,
               const std::shared_ptr<igl::IVertexInputState>& inputState,
               const std::shared_ptr<iglu::material::Material>& material) {
    IGL_ASSERT_MSG(sizeof(ImDrawIdx) == 2, "The constants below may not work with the ImGui data.");
    const size_t kMaxVertices = (1l << 16);
    const size_t kMaxVertexBufferSize = kMaxVertices * sizeof(ImDrawVert);
    const size_t kMaxIndexBufferSize = kMaxVertices * sizeof(ImDrawIdx);

    const igl::BufferDesc vbDesc(igl::BufferDesc::BufferTypeBits::Vertex,
                                 nullptr,
                                 kMaxVertexBufferSize,
                                 igl::ResourceStorage::Shared);
    const igl::BufferDesc ibDesc(igl::BufferDesc::BufferTypeBits::Index,
                                 nullptr,
                                 kMaxIndexBufferSize,
                                 igl::ResourceStorage::Shared);

    iglu::vertexdata::PrimitiveDesc primitiveDesc;
    primitiveDesc.numEntries = 0;

    vertexData = std::make_shared<iglu::vertexdata::VertexData>(
        inputState,
        device.createBuffer(vbDesc, nullptr),
        device.createBuffer(ibDesc, nullptr),
        sizeof(ImDrawIdx) == sizeof(uint16_t) ? igl::IndexFormat::UInt16 : igl::IndexFormat::UInt32,
        primitiveDesc);

    drawable = std::make_shared<iglu::drawable::Drawable>(vertexData, material);
  }
};

class Session::Renderer {
 public:
  Renderer(igl::IDevice& device);
  ~Renderer();

  void newFrame(const igl::FramebufferDesc& desc);
  void renderDrawData(igl::IDevice& device,
                      igl::IRenderCommandEncoder& cmdEncoder,
                      ImDrawData* drawData);

 private:
  std::shared_ptr<igl::IVertexInputState> _vertexInputState;
  std::shared_ptr<iglu::material::Material> _material;
  std::vector<DrawableData> _drawables[3]; // list of drawables to be reused every 3 frames
  size_t _nextBufferingIndex = 0;

  igl::RenderPipelineDesc _renderPipelineDesc;
  std::shared_ptr<igl::ITexture> _fontTexture;
  std::shared_ptr<igl::ISamplerState> _linearSampler;
};

Session::Renderer::Renderer(igl::IDevice& device) {
  ImGuiIO& io = ImGui::GetIO();
  io.BackendRendererName = "imgui_impl_igl";

  _linearSampler = device.createSamplerState(igl::SamplerStateDesc::newLinear(), nullptr);

  { // init fonts
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    igl::TextureDesc desc = igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                    width,
                                                    height,
                                                    igl::TextureDesc::TextureUsageBits::Sampled);
    desc.debugName = "IGLU/imgui/Session.cpp:Session::Renderer::_fontTexture";
    _fontTexture = device.createTexture(desc, nullptr);
    _fontTexture->upload(igl::TextureRangeDesc::new2D(0, 0, width, height), pixels);

    io.Fonts->TexID = reinterpret_cast<void*>(_fontTexture.get());
  }

  {
    igl::VertexInputStateDesc inputDesc;
    inputDesc.numAttributes = 3;
    inputDesc.attributes[0] = igl::VertexAttribute(
        0, igl::VertexAttributeFormat::Float2, offsetof(ImDrawVert, pos), "position", 0);
    inputDesc.attributes[1] = igl::VertexAttribute(
        0, igl::VertexAttributeFormat::Float2, offsetof(ImDrawVert, uv), "texCoords", 1);
    inputDesc.attributes[2] = igl::VertexAttribute(
        0, igl::VertexAttributeFormat::UByte4Norm, offsetof(ImDrawVert, col), "color", 2);
    inputDesc.numInputBindings = 1;
    inputDesc.inputBindings[0].stride = sizeof(ImDrawVert);
    _vertexInputState = device.createVertexInputState(inputDesc, nullptr);
  }

  {
    auto stages = getShaderStagesForBackend(device);
    auto program = std::make_shared<iglu::material::ShaderProgram>(
        device, std::move(stages), _vertexInputState);

    _material = std::make_shared<iglu::material::Material>(device, "imgui");
    _material->setShaderProgram(device, program);
    _material->cullMode = igl::CullMode::Disabled;
    _material->blendMode = iglu::material::BlendMode::Translucent();

    // @fb-only
    if (device.getBackendType() != igl::BackendType::Vulkan) {
      _material->shaderUniforms().setTexture("texture", _fontTexture.get(), _linearSampler);
    }
  }
}

Session::Renderer::~Renderer() {
  ImGuiIO& io = ImGui::GetIO();
  _fontTexture = nullptr;
  io.Fonts->TexID = nullptr;
}

void Session::Renderer::newFrame(const igl::FramebufferDesc& desc) {
  IGL_ASSERT(desc.colorAttachments[0].texture);
  _renderPipelineDesc.targetDesc.colorAttachments.resize(1);
  _renderPipelineDesc.targetDesc.colorAttachments[0].textureFormat =
      desc.colorAttachments[0].texture->getFormat();
  _renderPipelineDesc.targetDesc.depthAttachmentFormat =
      desc.depthAttachment.texture ? desc.depthAttachment.texture->getFormat()
                                   : igl::TextureFormat::Invalid;
  _renderPipelineDesc.targetDesc.stencilAttachmentFormat =
      desc.stencilAttachment.texture ? desc.stencilAttachment.texture->getFormat()
                                     : igl::TextureFormat::Invalid;
  _renderPipelineDesc.sampleCount = desc.colorAttachments[0].texture->getSamples();
}

void Session::Renderer::renderDrawData(igl::IDevice& device,
                                       igl::IRenderCommandEncoder& cmdEncoder,
                                       ImDrawData* drawData) {
  // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates !=
  // framebuffer coordinates)
  int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
  int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
  if (fb_width <= 0 || fb_height <= 0 || drawData->CmdListsCount == 0) {
    return;
  }

  cmdEncoder.pushDebugGroupLabel("ImGui Rendering", igl::Color(0, 1, 0));

  igl::Viewport viewport = {
      /*.x = */ 0.0,
      /*.y = */ 0.0,
      /*.width = */ (drawData->DisplaySize.x * drawData->FramebufferScale.x),
      /*.height = */ (drawData->DisplaySize.y * drawData->FramebufferScale.y),
  };
  cmdEncoder.bindViewport(viewport);

  using namespace iglu::simdtypes;
  float4x4 orthoProjection{};

  { // setup projection matrix
    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    orthoProjection.columns[0] = float4{2.0f / (R - L), 0.0f, 0.0f, 0.0f};
    orthoProjection.columns[1] = float4{0.0f, 2.0f / (T - B), 0.0f, 0.0f};
    orthoProjection.columns[2] = float4{0.0f, 0.0f, -1.0f, 0.0f};
    orthoProjection.columns[3] = float4{(R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f};
    if (device.getBackendType() != igl::BackendType::Vulkan) {
      _material->shaderUniforms().setFloat4x4(igl::genNameHandle("projectionMatrix"),
                                              orthoProjection);
    }
  }

  ImVec2 clip_off = drawData->DisplayPos; // (0,0) unless using multi-viewports
  ImVec2 clip_scale =
      drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

  // Since vertex buffers are updated every frame, we must use triple buffering for Metal to work
  std::vector<DrawableData>& curFrameDrawables = _drawables[_nextBufferingIndex];
  _nextBufferingIndex = (_nextBufferingIndex + 1) % 3;

  const bool isOpenGL = device.getBackendType() == igl::BackendType::OpenGL;
  const bool isVulkan = device.getBackendType() == igl::BackendType::Vulkan;

  ImTextureID lastBoundTextureId = nullptr;

  for (int n = 0; n < drawData->CmdListsCount; n++) {
    const ImDrawList* cmd_list = drawData->CmdLists[n];

    if (n >= curFrameDrawables.size()) {
      curFrameDrawables.emplace_back(device, _vertexInputState, _material);
    }
    DrawableData& drawableData = curFrameDrawables[n];

    // Upload vertex/index buffers
    drawableData.vertexData->vertexBuffer().upload(
        cmd_list->VtxBuffer.Data, {cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), 0});
    drawableData.vertexData->indexBuffer().upload(
        cmd_list->IdxBuffer.Data, {cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), 0});

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd cmd = cmd_list->CmdBuffer[cmd_i];
      IGL_ASSERT(cmd.UserCallback == nullptr);

      const ImVec2 clipMin((cmd.ClipRect.x - clip_off.x) * clip_scale.x,
                           (cmd.ClipRect.y - clip_off.y) * clip_scale.y);
      const ImVec2 clipMax((cmd.ClipRect.z - clip_off.x) * clip_scale.x,
                           (cmd.ClipRect.w - clip_off.y) * clip_scale.y);

      if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
        continue;
      }

      // OpenGL Y-axis goes up (Vulkan and Metal are good)
      // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
      const igl::ScissorRect rect{uint32_t(clipMin.x),
                                  isOpenGL ? uint32_t(viewport.height - clipMax.y)
                                           : uint32_t(clipMin.y),
                                  uint32_t(clipMax.x - clipMin.x),
                                  uint32_t(clipMax.y - clipMin.y)};
      cmdEncoder.bindScissorRect(rect);

      if (cmd.TextureId != lastBoundTextureId) {
        lastBoundTextureId = cmd.TextureId;
        auto* tex = reinterpret_cast<igl::ITexture*>(cmd.TextureId);
        if (isVulkan) {
          // @fb-only
          // Add Vulkan support for texture reflection info in ShaderUniforms so we don't need to
          // bind the texture directly
          cmdEncoder.bindTexture(0, igl::BindTarget::kFragment, tex);
          cmdEncoder.bindSamplerState(0, igl::BindTarget::kFragment, _linearSampler.get());
        } else {
          _material->shaderUniforms().setTexture(
              "texture", tex ? tex : _fontTexture.get(), _linearSampler);
        }
      }

      drawableData.vertexData->primitiveDesc().numEntries = cmd.ElemCount;
      drawableData.vertexData->primitiveDesc().offset = cmd.IdxOffset * sizeof(ImDrawIdx);

      drawableData.drawable->draw(device,
                                  cmdEncoder,
                                  _renderPipelineDesc,
                                  isVulkan ? sizeof(orthoProjection) : 0,
                                  &orthoProjection);
    }
  }

  if (isOpenGL) {
    // disable scissor
    cmdEncoder.bindScissorRect(igl::ScissorRect());
  }

  cmdEncoder.popDebugGroupLabel();
}

/* public API */

Session::Session(igl::IDevice& device,
                 igl::shell::InputDispatcher& inputDispatcher,
                 bool needInitializeSession /* = true */) :
  _inputDispatcher(inputDispatcher), _isInitialized(false) {
  _context = ImGui::CreateContext();
  makeCurrentContext();

  ImGuiStyle& style = ImGui::GetStyle();
  style.TouchExtraPadding = ImVec2(5, 5); // adjust to make touches more accurate

  if (needInitializeSession) {
    initialize(device);
  }
}

void Session::initialize(igl::IDevice& device) {
  if (!_isInitialized) {
    _inputListener = std::make_shared<InputListener>(_context);
    _renderer = std::make_unique<Renderer>(device);
    _inputDispatcher.addMouseListener(_inputListener);
    _inputDispatcher.addTouchListener(_inputListener);
    _isInitialized = true;
  }
}

Session::~Session() {
  makeCurrentContext();

  _inputDispatcher.removeTouchListener(_inputListener);
  _inputDispatcher.removeMouseListener(_inputListener);
  _renderer = nullptr;
  _inputListener = nullptr;
  ImGui::DestroyContext();
}

void Session::beginFrame(const igl::FramebufferDesc& desc, float displayScale) {
  makeCurrentContext();

  IGL_ASSERT(desc.colorAttachments[0].texture);

  const igl::Size size = desc.colorAttachments[0].texture->getSize();

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(size.width / displayScale, size.height / displayScale);
  io.DisplayFramebufferScale = ImVec2(displayScale, displayScale);
  io.IniFilename = nullptr;

  _renderer->newFrame(desc);
  ImGui::NewFrame();
}

void Session::endFrame(igl::IDevice& device, igl::IRenderCommandEncoder& cmdEncoder) {
  makeCurrentContext();

  ImGui::EndFrame();
  ImGui::Render();
  _renderer->renderDrawData(device, cmdEncoder, ImGui::GetDrawData());
}

void Session::makeCurrentContext() const {
  ImGui::SetCurrentContext(_context);
}

} // namespace imgui
} // namespace iglu
