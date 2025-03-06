/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <cstddef>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <shell/renderSessions/BindGroupSession.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace {
struct VertexPosUvw {
  glm::vec3 position;
  glm::vec3 color;
  glm::vec2 uv;
};

const float half = 1.0f;

// UV-mapped cube with indices: 24 vertices, 36 indices
const VertexPosUvw vertexData0[] = {
    // top
    {{-half, -half, +half}, {0.5, 0.5, 1.0}, {0, 0}}, // 0
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {1, 0}}, // 1
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 2
    {{-half, +half, +half}, {0.5, 1.0, 1.0}, {0, 1}}, // 3
    // bottom
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 4
    {{-half, +half, -half}, {0.5, 1.0, 0.5}, {0, 1}}, // 5
    {{+half, +half, -half}, {1.0, 1.0, 0.5}, {1, 1}}, // 6
    {{+half, -half, -half}, {1.0, 0.5, 0.5}, {1, 0}}, // 7
    // left
    {{+half, +half, -half}, {1.0, 1.0, 0.5}, {1, 0}}, // 8
    {{-half, +half, -half}, {0.5, 1.0, 0.5}, {0, 0}}, // 9
    {{-half, +half, +half}, {0.5, 1.0, 1.0}, {0, 1}}, // 10
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 11
    // right
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 12
    {{+half, -half, -half}, {1.0, 0.5, 0.5}, {1, 0}}, // 13
    {{+half, -half, +half}, {1.0, 0.5, 1.0}, {1, 1}}, // 14
    {{-half, -half, +half}, {0.5, 0.5, 1.0}, {0, 1}}, // 15
    // front
    {{+half, -half, -half}, {1.0, 0.5, 0.5}, {0, 0}}, // 16
    {{+half, +half, -half}, {1.0, 1.0, 0.5}, {1, 0}}, // 17
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 18
    {{+half, -half, +half}, {1.0, 0.5, 1.0}, {0, 1}}, // 19
    // back
    {{-half, +half, -half}, {0.5, 1.0, 0.5}, {1, 0}}, // 20
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 21
    {{-half, -half, +half}, {0.5, 0.5, 1.0}, {0, 1}}, // 22
    {{-half, +half, +half}, {0.5, 1.0, 1.0}, {1, 1}}, // 23
};

const uint16_t indexData[] = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                              8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                              16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

std::string getProlog(igl::IDevice& device) {
#if IGL_BACKEND_OPENGL
  const auto shaderVersion = device.getShaderVersion();
  if (shaderVersion.majorVersion >= 3 || shaderVersion.minorVersion >= 30) {
    std::string prependVersionString = igl::opengl::getStringFromShaderVersion(shaderVersion);
    prependVersionString += "\nprecision highp float;\n";
    return prependVersionString;
  }
#endif // IGL_BACKEND_OPENGL
  return "";
}

std::string getMetalShaderSource() {
  return R"(
          #include <metal_stdlib>
          #include <simd/simd.h>
          using namespace metal;

          struct VertexUniformBlock {
            float4x4 mvpMatrix;
          };

          struct VertexIn {
            float3 position [[attribute(0)]];
            float2 uv [[attribute(1)]];
            float3 color [[attribute(2)]];
          };

          struct VertexOut {
            float4 position [[position]];
            float2 uv;
            float4 color;
          };

          vertex VertexOut vertexShader(VertexIn in [[stage_in]],
                 constant VertexUniformBlock &vUniform[[buffer(1)]]) {
            VertexOut out;
            out.position = vUniform.mvpMatrix * float4(in.position, 1.0);
            out.uv = in.uv;
            out.color = float4(in.color, 1.0);
            return out;
           }

           fragment float4 fragmentShader(
                 VertexOut in[[stage_in]],
                 texture2d<float> tex0 [[texture(0)]],
                 texture2d<float> tex1 [[texture(1)]],
                 sampler linearSampler [[sampler(0)]]) {
             constexpr sampler s(s_address::clamp_to_edge,
                                 t_address::clamp_to_edge,
                                 min_filter::linear,
                                 mag_filter::linear);
             return tex0.sample(s, in.uv) * tex1.sample(s, in.uv) * in.color;
           }
        )";
}

std::string getOpenGLFragmentShaderSource(igl::IDevice& device) {
  return getProlog(device) + std::string(R"(
                      precision highp float; precision highp sampler2D;
                      in vec2 uv;
                      in vec4 color;
                      uniform sampler2D input2D;
                      uniform sampler2D inputXOR;
                      out vec4 fragmentColor;
                      void main() {
                        fragmentColor = texture(input2D, uv) * texture(inputXOR, uv) * color;
                      })");
}

std::string getOpenGLVertexShaderSource(igl::IDevice& device) {
  return getProlog(device) + R"(
                      precision highp float;
                      uniform mat4 mvpMatrix;
                      in vec3 position;
                      in vec2 uv_in;
                      in vec3 color_in;
                      out vec2 uv;
                      out vec4 color;

                      void main() {
                        gl_Position =  mvpMatrix * vec4(position, 1.0);
                        uv = uv_in;
                        color = vec4(color_in, 1.0);
                      })";
}

const char* getVulkanFragmentShaderSource() {
  return R"(
                      precision highp float;
                      layout(location = 0) in vec2 uv;
                      layout(location = 1) in vec4 color;
                      layout(location = 0) out vec4 out_FragColor;

                      layout(set = 0, binding = 0) uniform sampler2D in_texture0;
                      layout(set = 0, binding = 1) uniform sampler2D in_texture1;

                      void main() {
                        out_FragColor = texture(in_texture0, uv) * texture(in_texture1, uv) * color;
                      })";
}

const char* getVulkanVertexShaderSource() {
  return R"(
                      precision highp float;

                      layout (set = 1, binding = 1, std140) uniform PerFrame {
                        mat4 mvpMatrix;
                      } perFrame;

                      layout(location = 0) in vec3 position;
                      layout(location = 1) in vec2 uvw_in;
                      layout(location = 2) in vec3 color_in;
                      layout(location = 0) out vec2 uvw;
                      layout(location = 1) out vec4 color;

                      void main() {
                        gl_Position =  perFrame.mvpMatrix * vec4(position, 1.0);
                        uvw = uvw_in;
                        color = vec4(color_in, 1.0);
                      })";
}

std::unique_ptr<igl::IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource(),
                                                           "main",
                                                           "",
                                                           getVulkanFragmentShaderSource(),
                                                           "main",
                                                           "",
                                                           nullptr);
    return nullptr;
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, getMetalShaderSource().c_str(), "vertexShader", "fragmentShader", "", nullptr);
  case igl::BackendType::OpenGL:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getOpenGLVertexShaderSource(device).c_str(),
        "main",
        "",
        getOpenGLFragmentShaderSource(device).c_str(),
        "main",
        "",
        nullptr);
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

} // namespace

namespace igl::shell {

void BindGroupSession::createSamplerAndTextures(const igl::IDevice& device) {
  // Sampler & Texture
  auto sampler = device.createSamplerState(SamplerStateDesc::newLinearMipmapped(), nullptr);

  std::shared_ptr<ITexture> tex0, tex1;

  {
    auto imageData = getPlatform().getImageLoader().loadImageData("igl.png");
    TextureDesc desc = igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                               imageData.desc.width,
                                               imageData.desc.height,
                                               igl::TextureDesc::TextureUsageBits::Sampled |
                                                   TextureDesc::TextureUsageBits::Storage,
                                               "igl.png");
    desc.numMipLevels =
        igl::TextureDesc::calcNumMipLevels(imageData.desc.width, imageData.desc.height);
    tex0 = device.createTexture(desc, nullptr);
    tex0->upload(tex0->getFullRange(), imageData.data->data());
    tex0->generateMipmap(*commandQueue_);
  }

  {
    const uint32_t texWidth = 256;
    const uint32_t texHeight = 256;
    TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::BGRA_UNorm8,
                                          texWidth,
                                          texHeight,
                                          TextureDesc::TextureUsageBits::Sampled,
                                          "XOR pattern");
    desc.numMipLevels = igl::TextureDesc::calcNumMipLevels(texWidth, texHeight);
    tex1 = getPlatform().getDevice().createTexture(desc, nullptr);
    std::vector<uint32_t> pixels(static_cast<size_t>(texWidth * texHeight));
    for (uint32_t y = 0; y != texHeight; y++) {
      for (uint32_t x = 0; x != texWidth; x++) {
        // create a XOR pattern
        pixels[y * texWidth + x] = 0xFF000000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
      }
    }
    tex1->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels.data());
    tex1->generateMipmap(*commandQueue_);
  }

  bindGroupTextures_ = getPlatform().getDevice().createBindGroup(BindGroupTextureDesc{
      {tex0, tex1},
      {sampler, sampler},
      "bindGroupTextures_",
  });
}

BindGroupSession::BindGroupSession(std::shared_ptr<Platform> platform) :
  RenderSession(std::move(platform)) {
  imguiSession_ = std::make_unique<iglu::imgui::Session>(getPlatform().getDevice(),
                                                         getPlatform().getInputDispatcher());
}

void BindGroupSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Vertex buffer, Index buffer and Vertex Input
  const BufferDesc vb0Desc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData0, sizeof(vertexData0));
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 3;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[0].offset = offsetof(VertexPosUvw, position);
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
  inputDesc.attributes[1].offset = offsetof(VertexPosUvw, uv);
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].name = "uv_in";
  inputDesc.attributes[1].location = 1;
  inputDesc.attributes[2].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[2].offset = offsetof(VertexPosUvw, color);
  inputDesc.attributes[2].bufferIndex = 0;
  inputDesc.attributes[2].name = "color_in";
  inputDesc.attributes[2].location = 2;
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUvw);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);

  createSamplerAndTextures(device);
  shaderStages_ = getShaderStagesForBackend(device);

  // Command queue: backed by different types of GPU HW queues
  commandQueue_ = device.createCommandQueue({}, nullptr);

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.storeAction = StoreAction::DontCare;
  renderPass_.depthAttachment.clearDepth = 1.0;
}

void BindGroupSession::update(SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();

  const float deltaSeconds = getDeltaSeconds();

  fps_.updateFPS(deltaSeconds);

  // cube animation
  const glm::mat4 projectionMat = glm::perspectiveLH(
      glm::radians(45.0f), surfaceTextures.color->getAspectRatio(), 0.1f, 100.0f);
  angle_ += 180.0f * deltaSeconds;
  vertexParameters_.mvpMatrix =
      projectionMat * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.f, 8.0f)) *
      glm::rotate(glm::mat4(1.0f), -0.2f, glm::vec3(1.0f, 0.0f, 0.0f)) *
      glm::rotate(glm::mat4(1.0f), glm::radians(angle_), glm::vec3(0.0f, 1.0f, 0.0f));

  Result ret;
  if (framebuffer_ == nullptr) {
    framebufferDesc_.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc_.depthAttachment.texture = surfaceTextures.depth;
    framebuffer_ = device.createFramebuffer(framebufferDesc_, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  if (pipelineState_ == nullptr) {
    RenderPipelineDesc desc;
    desc.vertexInputState = vertexInput0_;
    desc.shaderStages = shaderStages_;
    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    desc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    desc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("input2D");
    desc.fragmentUnitSamplerMap[1] = IGL_NAMEHANDLE("inputXOR");
    desc.cullMode = igl::CullMode::Back;
    desc.frontFaceWinding = igl::WindingMode::Clockwise;
    pipelineState_ = device.createRenderPipeline(desc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
  }

  auto buffer = commandQueue_->createCommandBuffer({}, nullptr);

  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  // Bind Vertex Uniform Data
  iglu::ManagedUniformBufferInfo info;
  info.index = 1;
  info.length = sizeof(VertexFormat);
  info.uniforms = std::vector<UniformDesc>{UniformDesc{
      "mvpMatrix", -1, igl::UniformType::Mat4x4, 1, offsetof(VertexFormat, mvpMatrix), 0}};

  const std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer =
      std::make_shared<iglu::ManagedUniformBuffer>(device, info);
  IGL_DEBUG_ASSERT(vertUniformBuffer->result.isOk());
  *static_cast<VertexFormat*>(vertUniformBuffer->getData()) = vertexParameters_;
  vertUniformBuffer->bind(device, *pipelineState_, *commands);
  commands->bindBindGroup(bindGroupTextures_);
  commands->bindRenderPipelineState(pipelineState_);
  commands->bindVertexBuffer(0, *vb0_);
  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
  commands->drawIndexed(static_cast<size_t>(3u * 6u * 2u));

  imguiSession_->beginFrame(framebufferDesc_, getPlatform().getDisplayContext().pixelsPerPoint);
  imguiSession_->drawFPS(fps_.getAverageFPS());
  imguiSession_->endFrame(device, *commands);

  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_->getColorAttachment(0));
  }

  commandQueue_->submit(*buffer);
}

} // namespace igl::shell
