/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "Session.h"

#include <IGLU/simple_renderer/Drawable.h>
#include <IGLU/simple_renderer/Material.h>
#include <igl/ShaderCreator.h>

// D3D12 FXC precompiled shaders
#include "imgui_vs_d3d12_fxc.h"
#include "imgui_ps_d3d12_fxc.h"

namespace iglu::imgui {

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
#if IGL_PLATFORM_MACOSX
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
#if IGL_PLATFORM_MACOSX
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

static const char* getD3D12VertexShaderSource() {
  return R"(
cbuffer Uniforms : register(b0) {
  float4x4 projectionMatrix;
};

struct VSInput {
  float2 position : POSITION;
  float2 uv : TEXCOORD0;
  float4 color : COLOR;
};

struct PSInput {
  float4 position : SV_Position;
  float4 color : COLOR;
  float2 uv : TEXCOORD0;
};

PSInput main(VSInput input) {
  PSInput output;
  // Column-major multiplication to match the CPU-side matrix format
  // In HLSL: mul(vector, matrix) treats matrix as column-major
  output.position = mul(float4(input.position.xy, 0, 1), projectionMatrix);
  output.color = input.color;
  output.uv = input.uv;
  return output;
})";
}

static const char* getD3D12FragmentShaderSource() {
  return R"(
struct PSInput {
  float4 position : SV_Position;
  float4 color : COLOR;
  float2 uv : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState uSampler : register(s0);

float4 main(PSInput input) : SV_Target {
  return input.color * tex.Sample(uSampler, input.uv);
})";
}

static std::unique_ptr<igl::IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  igl::Result result;
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
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
  case igl::BackendType::D3D12: {
    return igl::ShaderStagesCreator::fromModuleBinaryInput(device,
                                                           _tmp_imgui_vs_fxc_cso,
                                                           _tmp_imgui_vs_fxc_cso_len,
                                                           "main",
                                                           "Shader Module: imgui::vertex (D3D12)",
                                                           _tmp_imgui_ps_fxc_cso,
                                                           _tmp_imgui_ps_fxc_cso_len,
                                                           "main",
                                                           "Shader Module: imgui::fragment (D3D12)",
                                                           &result);
    break;
  }
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Custom:
    IGL_DEBUG_ABORT("IGLSamples not set up for Custom");
    return nullptr;
  case igl::BackendType::Metal: {
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, metalShaderStr(), "vertex_main", "fragment_main", "", &result);
    break;
  }
  case igl::BackendType::OpenGL: {
    auto shaderVersion = device.getShaderVersion();
    const std::string vertexStr = getOpenGLVertexShaderSource(shaderVersion);
    const std::string fragmentStr = getOpenGLFragmentShaderSource(shaderVersion);
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, vertexStr.c_str(), "main", "", fragmentStr.c_str(), "main", "", &result);
    break;
  }
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

namespace {
struct DrawableData {
  std::shared_ptr<iglu::vertexdata::VertexData> vertexData;
  std::shared_ptr<iglu::drawable::Drawable> drawable;

  DrawableData(igl::IDevice& device,
               const std::shared_ptr<igl::IVertexInputState>& inputState,
               const std::shared_ptr<iglu::material::Material>& material) {
    IGL_DEBUG_ASSERT(sizeof(ImDrawIdx) == 2,
                     "The constants below may not work with the ImGui data.");
    const size_t kMaxVertices = (1l << 16);
    const size_t kMaxVertexBufferSize = kMaxVertices * sizeof(ImDrawVert);
    const size_t kMaxIndexBufferSize = kMaxVertices * sizeof(ImDrawIdx);

    const igl::BufferDesc vbDesc(igl::BufferDesc::BufferTypeBits::Vertex,
                                 nullptr,
                                 kMaxVertexBufferSize,
                                 igl::ResourceStorage::Shared,
                                 0,
                                 "vertex (" + material->name + ")");
    const igl::BufferDesc ibDesc(igl::BufferDesc::BufferTypeBits::Index,
                                 nullptr,
                                 kMaxIndexBufferSize,
                                 igl::ResourceStorage::Shared,
                                 0,
                                 "index (" + material->name + ")");

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
} // namespace

class Session::Renderer {
 public:
  explicit Renderer(igl::IDevice& device);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;

  void newFrame(const igl::FramebufferDesc& desc);
  void renderDrawData(igl::IDevice& device,
                      igl::IRenderCommandEncoder& cmdEncoder,
                      ImDrawData* drawData);

 private:
  std::shared_ptr<igl::IVertexInputState> vertexInputState_;
  std::shared_ptr<iglu::material::Material> material_;
  std::vector<DrawableData> drawables_[3]; // list of drawables to be reused every 3 frames
  size_t nextBufferingIndex_ = 0;

  igl::RenderPipelineDesc renderPipelineDesc_;
  std::shared_ptr<igl::ITexture> fontTexture_;
  std::shared_ptr<igl::ISamplerState> linearSampler_;
};

Session::Renderer::Renderer(igl::IDevice& device) {
  ImGuiIO& io = ImGui::GetIO();
  io.BackendRendererName = "imgui_impl_igl";

  linearSampler_ = device.createSamplerState(igl::SamplerStateDesc::newLinear(), nullptr);

  { // init fonts
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    igl::TextureDesc desc = igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                    width,
                                                    height,
                                                    igl::TextureDesc::TextureUsageBits::Sampled);
    desc.debugName = "IGLU/imgui/Session.cpp:Session::Renderer::_fontTexture";
    fontTexture_ = device.createTexture(desc, nullptr);
    fontTexture_->upload(igl::TextureRangeDesc::new2D(0, 0, width, height), pixels);

    io.Fonts->TexID = reinterpret_cast<void*>(fontTexture_.get());
  }

  {
    igl::VertexInputStateDesc inputDesc;
    inputDesc.numAttributes = 3;
    inputDesc.attributes[0] = igl::VertexAttribute{
        .bufferIndex = 0,
        .format = igl::VertexAttributeFormat::Float2,
        .offset = offsetof(ImDrawVert, pos),
        .name = "position",
        .location = 0,
    };
    inputDesc.attributes[1] = igl::VertexAttribute{
        .bufferIndex = 0,
        .format = igl::VertexAttributeFormat::Float2,
        .offset = offsetof(ImDrawVert, uv),
        .name = "texCoords",
        .location = 1,
    };
    inputDesc.attributes[2] = igl::VertexAttribute{
        .bufferIndex = 0,
        .format = igl::VertexAttributeFormat::UByte4Norm,
        .offset = offsetof(ImDrawVert, col),
        .name = "color",
        .location = 2,
    };
    inputDesc.numInputBindings = 1;
    inputDesc.inputBindings[0].stride = sizeof(ImDrawVert);
    vertexInputState_ = device.createVertexInputState(inputDesc, nullptr);
  }

  {
    auto stages = getShaderStagesForBackend(device);
    auto program = std::make_shared<iglu::material::ShaderProgram>(
        device, std::move(stages), vertexInputState_);

    material_ = std::make_shared<iglu::material::Material>(device, "imgui");
    material_->setShaderProgram(device, program);
    material_->cullMode = igl::CullMode::Disabled;
    material_->blendMode = iglu::material::BlendMode::Translucent();

    // @fb-only
    // D3D12 and Vulkan use direct slot binding, OpenGL/Metal use named binding
    const bool usesDirectBinding = (device.getBackendType() == igl::BackendType::Vulkan ||
                                     device.getBackendType() == igl::BackendType::D3D12);
    if (!usesDirectBinding) {
      material_->shaderUniforms().setTexture("texture", fontTexture_.get(), linearSampler_);
    }
  }
}

Session::Renderer::~Renderer() {
  const ImGuiIO& io = ImGui::GetIO();
  fontTexture_ = nullptr;
  io.Fonts->TexID = nullptr;
}

void Session::Renderer::newFrame(const igl::FramebufferDesc& desc) {
  IGL_DEBUG_ASSERT(desc.colorAttachments[0].texture);
  renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
  renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
      desc.colorAttachments[0].texture->getFormat();
  renderPipelineDesc_.targetDesc.depthAttachmentFormat =
      desc.depthAttachment.texture ? desc.depthAttachment.texture->getFormat()
                                   : igl::TextureFormat::Invalid;
  renderPipelineDesc_.targetDesc.stencilAttachmentFormat =
      desc.stencilAttachment.texture ? desc.stencilAttachment.texture->getFormat()
                                     : igl::TextureFormat::Invalid;
  renderPipelineDesc_.sampleCount = desc.colorAttachments[0].texture->getSamples();
}

void Session::Renderer::renderDrawData(igl::IDevice& device,
                                       igl::IRenderCommandEncoder& cmdEncoder,
                                       ImDrawData* drawData) {
  // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates !=
  // framebuffer coordinates)
  const int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
  const int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);

  IGL_LOG_INFO("ImGui renderDrawData: DisplaySize=(%.1f,%.1f), FramebufferScale=(%.1f,%.1f), fb=(%d,%d), CmdLists=%d, TotalVtx=%d, TotalIdx=%d\n",
               drawData->DisplaySize.x, drawData->DisplaySize.y,
               drawData->FramebufferScale.x, drawData->FramebufferScale.y,
               fbWidth, fbHeight, drawData->CmdListsCount, drawData->TotalVtxCount, drawData->TotalIdxCount);

  if (fbWidth <= 0 || fbHeight <= 0 || drawData->CmdListsCount == 0) {
    IGL_LOG_INFO("ImGui renderDrawData: Early return (invalid dimensions or no command lists)\n");
    return;
  }

  cmdEncoder.pushDebugGroupLabel("ImGui Rendering", igl::Color(0, 1, 0));

  const igl::Viewport viewport = {
      /*.x = */ 0.0,
      /*.y = */ 0.0,
      /*.width = */ (drawData->DisplaySize.x * drawData->FramebufferScale.x),
      /*.height = */ (drawData->DisplaySize.y * drawData->FramebufferScale.y),
  };
  cmdEncoder.bindViewport(viewport);

  using namespace iglu::simdtypes;
  float4x4 orthoProjection{};

  { // setup projection matrix
    const float l = drawData->DisplayPos.x;
    const float r = drawData->DisplayPos.x + drawData->DisplaySize.x;
    const float t = drawData->DisplayPos.y;
    const float b = drawData->DisplayPos.y + drawData->DisplaySize.y;
    orthoProjection.columns[0] = float4{2.0f / (r - l), 0.0f, 0.0f, 0.0f};
    orthoProjection.columns[1] = float4{0.0f, 2.0f / (t - b), 0.0f, 0.0f};
    orthoProjection.columns[2] = float4{0.0f, 0.0f, -1.0f, 0.0f};
    orthoProjection.columns[3] = float4{(r + l) / (l - r), (t + b) / (b - t), 0.0f, 1.0f};
    // D3D12 and Vulkan use direct slot binding, OpenGL/Metal use named binding
    const bool usesDirectBinding = (device.getBackendType() == igl::BackendType::Vulkan ||
                                     device.getBackendType() == igl::BackendType::D3D12);
    if (!usesDirectBinding) {
      material_->shaderUniforms().setFloat4x4(igl::genNameHandle("projectionMatrix"),
                                              orthoProjection);
    }
  }

  const ImVec2 clipOff = drawData->DisplayPos; // (0,0) unless using multi-viewports
  const ImVec2 clipScale =
      drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

  // Since vertex buffers are updated every frame, we must use triple buffering for Metal to work
  std::vector<DrawableData>& curFrameDrawables = drawables_[nextBufferingIndex_];
  nextBufferingIndex_ = (nextBufferingIndex_ + 1) % 3;

  const bool isOpenGL = device.getBackendType() == igl::BackendType::OpenGL;
  const bool isVulkan = device.getBackendType() == igl::BackendType::Vulkan;
  const bool isD3D12 = device.getBackendType() == igl::BackendType::D3D12;
  const bool usesDirectBinding = isVulkan || isD3D12;

  ImTextureID lastBoundTextureId = nullptr;

  for (int n = 0; n < drawData->CmdListsCount; n++) {
    const ImDrawList* cmdList = drawData->CmdLists[n];

    if (n >= curFrameDrawables.size()) {
      curFrameDrawables.emplace_back(device, vertexInputState_, material_);
    }
    const DrawableData& drawableData = curFrameDrawables[n];

    // Upload vertex/index buffers
    drawableData.vertexData->vertexBuffer().upload(
        cmdList->VtxBuffer.Data, {cmdList->VtxBuffer.Size * sizeof(ImDrawVert), 0});
    drawableData.vertexData->indexBuffer().upload(cmdList->IdxBuffer.Data,
                                                  {cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), 0});

    for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++) {
      const ImDrawCmd cmd = cmdList->CmdBuffer[cmdI];
      IGL_DEBUG_ASSERT(cmd.UserCallback == nullptr);

      const ImVec2 clipMin((cmd.ClipRect.x - clipOff.x) * clipScale.x,
                           (cmd.ClipRect.y - clipOff.y) * clipScale.y);
      const ImVec2 clipMax((cmd.ClipRect.z - clipOff.x) * clipScale.x,
                           (cmd.ClipRect.w - clipOff.y) * clipScale.y);

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
        if (usesDirectBinding) {
          // D3D12 and Vulkan use direct slot binding
          // @fb-only
          // Add Vulkan support for texture reflection info in ShaderUniforms so we don't need to
          // bind the texture directly
          cmdEncoder.bindTexture(0, igl::BindTarget::kFragment, tex);
          cmdEncoder.bindSamplerState(0, igl::BindTarget::kFragment, linearSampler_.get());
        } else {
          material_->shaderUniforms().setTexture(
              "texture", tex ? tex : fontTexture_.get(), linearSampler_);
        }
      }

      drawableData.vertexData->primitiveDesc().numEntries = cmd.ElemCount;
      drawableData.vertexData->primitiveDesc().offset = cmd.IdxOffset * sizeof(ImDrawIdx);

      drawableData.drawable->draw(device,
                                  cmdEncoder,
                                  renderPipelineDesc_,
                                  usesDirectBinding ? sizeof(orthoProjection) : 0,
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
  inputDispatcher_(inputDispatcher) {
  context_ = ImGui::CreateContext();
  makeCurrentContext();

  ImGuiStyle& style = ImGui::GetStyle();
  style.TouchExtraPadding = ImVec2(5, 5); // adjust to make touches more accurate

  if (needInitializeSession) {
    initialize(device);
  }
}

void Session::initialize(igl::IDevice& device) {
  if (!isInitialized_) {
    inputListener_ = std::make_shared<InputListener>(context_);
    renderer_ = std::make_unique<Renderer>(device);
    inputDispatcher_.addMouseListener(inputListener_);
    inputDispatcher_.addTouchListener(inputListener_);
    inputDispatcher_.addKeyListener(inputListener_);
    isInitialized_ = true;
  }
}

Session::~Session() {
  makeCurrentContext();

  inputDispatcher_.removeTouchListener(inputListener_);
  inputDispatcher_.removeMouseListener(inputListener_);
  inputDispatcher_.removeKeyListener(inputListener_);
  renderer_ = nullptr;
  inputListener_ = nullptr;
  ImGui::DestroyContext();
}

void Session::beginFrame(const igl::FramebufferDesc& desc, float displayScale) {
  makeCurrentContext();

  IGL_DEBUG_ASSERT(desc.colorAttachments[0].texture);

  const igl::Size size = desc.colorAttachments[0].texture->getSize();

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(size.width / displayScale, size.height / displayScale);
  io.DisplayFramebufferScale = ImVec2(displayScale, displayScale);
  io.IniFilename = nullptr;

  renderer_->newFrame(desc);
  ImGui::NewFrame();
}

void Session::endFrame(igl::IDevice& device, igl::IRenderCommandEncoder& cmdEncoder) {
  makeCurrentContext();

  ImGui::EndFrame();
  ImGui::Render();
  renderer_->renderDrawData(device, cmdEncoder, ImGui::GetDrawData());
}

void Session::makeCurrentContext() const {
  ImGui::SetCurrentContext(context_);
}

void Session::drawFPS(float fps) const {
  // a nice FPS counter
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove;
  const ImGuiViewport* v = ImGui::GetMainViewport();
  IGL_DEBUG_ASSERT(v);
  ImGui::SetNextWindowPos(
      {
          v->WorkPos.x + v->WorkSize.x - 15.0f,
          v->WorkPos.y + 15.0f,
      },
      ImGuiCond_Always,
      {1.0f, 0.0f});
  ImGui::SetNextWindowBgAlpha(0.30f);
  ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));
  if (ImGui::Begin("##FPS", nullptr, flags)) {
    ImGui::Text("FPS : %i", (int)fps);
    ImGui::Text("Ms  : %.1f", 1000.0 / fps);
  }
  ImGui::End();
}

} // namespace iglu::imgui
