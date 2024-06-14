/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/simdtypes/SimdTypes.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/GLIncludes.h>
#include <shell/renderSessions/YUVColorSession.h>
#include <shell/shared/fileLoader/FileLoader.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

struct VertexPosUv {
  iglu::simdtypes::float3 position;
  iglu::simdtypes::float2 uv;
};

namespace {

const VertexPosUv vertexData[] = {
    {{-1.f, 1.f, 0.0}, {0.0, 0.0}},
    {{1.f, 1.f, 0.0}, {1.0, 0.0}},
    {{-1.f, -1.f, 0.0}, {0.0, 1.0}},
    {{1.f, -1.f, 0.0}, {1.0, 1.0}},
};
const uint16_t indexData[] = {
    0,
    1,
    2,
    1,
    3,
    2,
};

const std::string getVersion() {
  return "#version 300 es";
}

const std::string getMetalShaderSource() {
  return R"(
              using namespace metal;

              typedef struct { float3 color; } UniformBlock;

              typedef struct {
                float3 position [[attribute(0)]];
                float2 uv [[attribute(1)]];
              } VertexIn;

              typedef struct {
                float4 position [[position]];
                float2 uv;
              } VertexOut;

              vertex VertexOut vertexShader(
                  uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(1)]]) {
                VertexOut out;
                out.position = float4(vertices[vid].position, 1.0);
                out.uv = vertices[vid].uv;
                return out;
              }

              fragment float4 fragmentShader(
                  VertexOut IN [[stage_in]],
                  texture2d<float> diffuseTex [[texture(0)]],
                  sampler linearSampler [[sampler(0)]],
                  constant UniformBlock * color [[buffer(0)]]) {
                float4 tex = diffuseTex.sample(linearSampler, IN.uv);
                return float4(color->color.r, color->color.g, color->color.b, 1.0) *
                      tex;
              }
    )";
}

const std::string getOpenGLVertexShaderSource() {
  return getVersion() + R"(
                precision highp float;
                in vec3 position;
                in vec2 uv_in;

                out vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in; // position.xy * 0.5 + 0.5;
                })";
}

const std::string getOpenGLFragmentShaderSource() {
  return getVersion() + std::string(R"(
                #extension GL_EXT_YUV_target : require
                precision highp float;
                uniform vec3 color;
                uniform sampler2D inputImage;

                in vec2 uv;
                layout (yuv) out vec4 outColor;

                void main() {
                  outColor =
                      vec4(color, 1.0) * texture(inputImage, uv);
                })");
}

const std::string getVulkanVertexShaderSource() {
  return R"(
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec2 uv_in;
                layout(location = 0) out vec2 uv;
                layout(location = 1) out vec3 color;

                layout (set = 1, binding = 0, std140) uniform UniformsPerObject {
                  vec3 color;
                } perObject;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in;
                  color = perObject.color;
                }
                )";
}

const std::string getVulkanFragmentShaderSource() {
  return R"(
                layout(location = 0) in vec2 uv;
                layout(location = 1) in vec3 color;
                layout(location = 0) out vec4 out_FragColor;

                layout(set = 0, binding = 0) uniform sampler2D in_texture;

                void main() {
                  out_FragColor = vec4(color, 1.0) * texture(in_texture, uv);
                }
                )";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
  // @fb-only
    IGL_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getVulkanFragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, getMetalShaderSource().c_str(), "vertexShader", "fragmentShader", "", nullptr);
  case igl::BackendType::OpenGL:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getOpenGLVertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getOpenGLFragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

} // namespace

YUVColorSession::YUVColorSession(std::shared_ptr<Platform> platform) :
  RenderSession(std::move(platform)) {}

// clang-tidy off
void YUVColorSession::initialize() noexcept {
  // clang-tidy on
  auto& device = getPlatform().getDevice();

  // Vertex & Index buffer
  vb0_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData)), nullptr);
  IGL_ASSERT(vb0_ != nullptr);
  ib0_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData)), nullptr);
  IGL_ASSERT(ib0_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = VertexAttribute(
      1, VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position", 0);
  inputDesc.attributes[1] =
      VertexAttribute(1, VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in", 1);
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[1].stride = sizeof(VertexPosUv);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);
  IGL_ASSERT(vertexInput0_ != nullptr);

  // Sampler & Texture
  samp0_ = device.createSamplerState(SamplerStateDesc::newLinear(), nullptr);
  IGL_ASSERT(samp0_ != nullptr);

  // https://github.com/facebook/igl/blob/main/shell/resources/images/output_frame_900.txt
  const auto fileData = getPlatform().getFileLoader().loadBinaryData("output_frame_900.yuv");

  const uint32_t width = 1920;
  const uint32_t height = 1080;

  const igl::TextureDesc textureDesc =
      igl::TextureDesc::new2D(igl::TextureFormat::R_UNorm8,
                              width,
                              height,
                              TextureDesc::TextureUsageBits::Sampled,
                              "YUV texture");
  IGL_ASSERT(textureDesc.width * textureDesc.height + textureDesc.width * textureDesc.height / 2 ==
             fileData.length);
  tex0_ = device.createTexture(textureDesc, nullptr);
  tex0_->upload(TextureRangeDesc{0, 0, 0, textureDesc.width, textureDesc.height},
                fileData.data.get());

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_ASSERT(shaderStages_ != nullptr);

  // Command queue
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);
  IGL_ASSERT(commandQueue_ != nullptr);

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPlatform().getDevice().backendDebugColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;

  // init uniforms
  fragmentParameters_ = FragmentFormat{{1.0f, 1.0f, 1.0f}};

  BufferDesc fpDesc;
  fpDesc.type = BufferDesc::BufferTypeBits::Uniform;
  fpDesc.data = &fragmentParameters_;
  fpDesc.length = sizeof(fragmentParameters_);
  fpDesc.storage = ResourceStorage::Shared;

  fragmentParamBuffer_ = device.createBuffer(fpDesc, nullptr);
  IGL_ASSERT(fragmentParamBuffer_ != nullptr);
}

void YUVColorSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  igl::Result ret;
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
    IGL_ASSERT(ret.isOk());
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_ASSERT(ret.isOk());
    IGL_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  const size_t _textureUnit = 0;

  // Graphics pipeline
  if (!pipelineState_) {
    RenderPipelineDesc desc;
    desc.vertexInputState = vertexInput0_;
    desc.shaderStages = shaderStages_;
    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    desc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    desc.fragmentUnitSamplerMap[_textureUnit] = IGL_NAMEHANDLE("inputImage");
    desc.cullMode = igl::CullMode::Back;
    desc.frontFaceWinding = igl::WindingMode::Clockwise;

    pipelineState_ = getPlatform().getDevice().createRenderPipeline(desc, nullptr);
    IGL_ASSERT(pipelineState_ != nullptr);

    // Set up uniform descriptors
    fragmentUniformDescriptors_.emplace_back();
  }

  // Command Buffers
  auto buffer = commandQueue_->createCommandBuffer({}, nullptr);
  IGL_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);

  framebuffer_->updateDrawable(drawableSurface);

  // Uniform: "color"
  if (!fragmentUniformDescriptors_.empty()) {
    fragmentUniformDescriptors_.back().type = UniformType::Float3;
    fragmentUniformDescriptors_.back().offset = offsetof(FragmentFormat, color);
  }

  // Submit commands
  std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindVertexBuffer(0, *vb0_);
    commands->bindVertexBuffer(1, *vb0_);
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindBuffer(0, fragmentParamBuffer_, 0);
    commands->bindTexture(_textureUnit, BindTarget::kFragment, tex0_.get());
    commands->bindSamplerState(_textureUnit, BindTarget::kFragment, samp0_.get());
    commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
    commands->drawIndexed(6);

    commands->endEncoding();
  }

  IGL_ASSERT(buffer != nullptr);
  buffer->present(drawableSurface);

  IGL_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
