/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>

#include <algorithm>
#include <cmath>
#include <glm/detail/qualifier.hpp>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <shell/renderSessions/HelloOpenXRSession.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

struct VertexPosUvw {
  glm::vec3 position;
  glm::vec3 uvw;
};

namespace {

const float half = 1.0f;
VertexPosUvw vertexData0[] = {
    {{-half, half, -half}, {0.0, 1.0, 0.0}},
    {{half, half, -half}, {1.0, 1.0, 0.0}},
    {{-half, -half, -half}, {0.0, 0.0, 0.0}},
    {{half, -half, -half}, {1.0, 0.0, 0.0}},
    {{half, half, half}, {1.0, 1.0, 1.0}},
    {{-half, half, half}, {0.0, 1.0, 1.0}},
    {{half, -half, half}, {1.0, 0.0, 1.0}},
    {{-half, -half, half}, {0.0, 0.0, 1.0}},
};
uint16_t indexData[] = {0, 1, 2, 1, 3, 2, 1, 4, 3, 4, 6, 3, 4, 5, 6, 5, 7, 6,
                        5, 0, 7, 0, 2, 7, 5, 4, 0, 4, 1, 0, 2, 3, 7, 3, 6, 7};

std::string getProlog(igl::IDevice& device) {
#if IGL_BACKEND_OPENGL
  const auto shaderVersion = device.getShaderVersion();
  if (shaderVersion.majorVersion >= 3 || shaderVersion.minorVersion >= 30) {
    std::string prependVersionString = igl::opengl::getStringFromShaderVersion(shaderVersion);
    prependVersionString += "\n#extension GL_OVR_multiview2 : require\n";
    prependVersionString += "\nprecision highp float;\n";
    return prependVersionString;
  }
#endif // IGL_BACKEND_OPENGL
  return "";
};

std::string getOpenGLFragmentShaderSource(igl::IDevice& device) {
  return getProlog(device) + std::string(R"(
                      precision highp float;
                      precision highp sampler2D;
                      in vec3 uvw;
                      in vec3 color;
                      uniform sampler2D inputImage;
                      out vec4 fragmentColor;
                      void main() {
                        fragmentColor = texture(inputImage, uvw.xy) * vec4(color, 1.0);
                      })");
}

std::string getOpenGLVertexShaderSource(igl::IDevice& device) {
  return getProlog(device) + R"(
                      layout(num_views = 2) in;
                      precision highp float;

                      uniform mat4 modelMatrix;
                      uniform mat4 viewProjectionMatrix[2];
                      uniform float scaleZ;

                      in vec3 position;
                      in vec3 uvw_in;
                      out vec3 uvw;
                      out vec3 color;

                      void main() {
                        mat4 mvpMatrix = viewProjectionMatrix[gl_ViewID_OVR] * modelMatrix;
                        gl_Position = mvpMatrix * vec4(position, 1.0);
                        uvw = vec3(uvw_in.x, uvw_in.y, (uvw_in.z - 0.5) * scaleZ + 0.5);
                        color = vec3(1.0, 1.0, 0.0);
                      })";
}

static const char* getVulkanFragmentShaderSource() {
  return R"(
                      precision highp float;
                      precision highp sampler2D;

                      layout(location = 0) in vec3 uvw;
                      layout(location = 1) in vec3 color;
                      layout(set = 0, binding = 0) uniform sampler2D inputImage;
                      layout(location = 0) out vec4 fragmentColor;

                      void main() {
                        fragmentColor = texture(inputImage, uvw.xy) * vec4(color, 1.0);
                      })";
}

static const char* getVulkanVertexShaderSource() {
  return R"(
                      #extension GL_OVR_multiview2 : require
                      layout(num_views = 2) in;
                      precision highp float;

                      layout (set = 1, binding = 1, std140) uniform PerFrame {
                        mat4 modelMatrix;
                        mat4 viewProjectionMatrix[2];
                        float scaleZ;
                      } perFrame;

                      layout(location = 0) in vec3 position;
                      layout(location = 1) in vec3 uvw_in;
                      layout(location = 0) out vec3 uvw;
                      layout(location = 1) out vec3 color;

                      void main() {
                        mat4 mvpMatrix = perFrame.viewProjectionMatrix[gl_ViewID_OVR] * perFrame.modelMatrix;
                        gl_Position = mvpMatrix * vec4(position, 1.0);
                        uvw = vec3(uvw_in.x, uvw_in.y, (uvw_in.z - 0.5) * perFrame.scaleZ + 0.5);
                        color = vec3(1.0, 1.0, 0.0);
                      })";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource(),
                                                           "main",
                                                           "",
                                                           getVulkanFragmentShaderSource(),
                                                           "main",
                                                           "",
                                                           nullptr);
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
  default:
    IGL_ASSERT_NOT_REACHED();
    return nullptr;
  }
}

bool isDeviceCompatible(IDevice& device) noexcept {
  return device.hasFeature(DeviceFeatures::Multiview);
}

glm::mat4 perspectiveAsymmetricFovRH(const igl::shell::Fov& fov, float nearZ, float farZ) {
  glm::mat4 mat;

  const float tanLeft = tanf(fov.angleLeft);
  const float tanRight = tanf(fov.angleRight);
  const float tanDown = tanf(fov.angleDown);
  const float tanUp = tanf(fov.angleUp);

  const float tanWidth = tanRight - tanLeft;
  const float tanHeight = tanUp - tanDown;

  mat[0][0] = 2.0f / tanWidth;
  mat[1][0] = 0.0f;
  mat[2][0] = (tanRight + tanLeft) / tanWidth;
  mat[3][0] = 0.0f;

  mat[0][1] = 0.0f;
  mat[1][1] = 2.0f / tanHeight;
  mat[2][1] = (tanUp + tanDown) / tanHeight;
  mat[3][1] = 0.0f;

  mat[0][2] = 0.0f;
  mat[1][2] = 0.0f;
  mat[2][2] = -(farZ + nearZ) / (farZ - nearZ);
  mat[3][2] = -2.0f * farZ * nearZ / (farZ - nearZ);

  mat[0][3] = 0.0f;
  mat[1][3] = 0.0f;
  mat[2][3] = -1.0f;
  mat[3][3] = 0.0f;

  return mat;
}
} // namespace

void HelloOpenXRSession::createSamplerAndTextures(const igl::IDevice& device) {
  // Sampler & Texture
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.addressModeU = SamplerAddressMode::MirrorRepeat;
  samplerDesc.addressModeV = SamplerAddressMode::MirrorRepeat;
  samplerDesc.addressModeW = SamplerAddressMode::MirrorRepeat;
  samp0_ = device.createSamplerState(samplerDesc, nullptr);

  tex0_ = getPlatform().loadTexture("macbeth.png");
}

void HelloOpenXRSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }
  // Vertex buffer, Index buffer and Vertex Input
  const BufferDesc vb0Desc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData0, sizeof(vertexData0));
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[0].offset = offsetof(VertexPosUvw, position);
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[1].offset = offsetof(VertexPosUvw, uvw);
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].name = "uvw_in";
  inputDesc.attributes[1].location = 1;
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUvw);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);

  createSamplerAndTextures(device);
  shaderStages_ = getShaderStagesForBackend(device);

  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);

  // Set up vertex uniform data
  vertexParameters_.scaleZ = 1.0f;

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
#if defined(IGL_OPENXR_MR_MODE)
  renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 1.0, 0.0f};
#else
  renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 1.0, 1.0f};
#endif
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
}

void HelloOpenXRSession::setVertexParams() {
  // rotating animation
  static float angle = 0.0f, scaleZ = 1.0f, ss = 0.005f;
  angle += 0.005f;
  scaleZ += ss;
  scaleZ = scaleZ < 0.0f ? 0.0f : (scaleZ > 1.0 ? 1.0f : scaleZ);
  if (scaleZ <= 0.05f || scaleZ >= 1.0f) {
    ss *= -1.0f;
  }

  glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)) *
                     glm::rotate(glm::mat4(1.0f), -0.2f, glm::vec3(1.0f, 0.0f, 0.0f));
  vertexParameters_.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.f, -8.0f)) *
                                  rotMat *
                                  glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, scaleZ));
  for (size_t i = 0; i < std::min(shellParams().viewParams.size(), size_t(2)); ++i) {
    vertexParameters_.viewProjectionMatrix[i] =
        perspectiveAsymmetricFovRH(shellParams().viewParams[i].fov, 0.1f, 100.0f) *
        shellParams().viewParams[i].viewMatrix;
  }

  vertexParameters_.scaleZ = scaleZ;
}

void HelloOpenXRSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  // cube animation
  setVertexParams();

  igl::Result ret;
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;

    framebufferDesc.mode = surfaceTextures.color->getNumLayers() > 1 ? FramebufferMode::Stereo
                                                                     : FramebufferMode::Mono;

    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_ASSERT(ret.isOk());
    IGL_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  constexpr uint32_t textureUnit = 0;
  if (pipelineState_ == nullptr) {
    // Graphics pipeline: state batch that fully configures GPU for rendering

    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[textureUnit] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
  }

  // Command buffers (1-N per thread): create, submit and forget
  auto buffer = commandQueue_->createCommandBuffer(CommandBufferDesc{}, nullptr);
  const std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  commands->pushDebugGroupLabel("HelloOpenXRSession Commands", igl::Color(0.0f, 1.0f, 0.0f));

  commands->bindVertexBuffer(0, *vb0_);

  // Bind Vertex Uniform Data
  iglu::ManagedUniformBufferInfo info;
  info.index = 1;
  info.length = sizeof(VertexFormat);
  info.uniforms = std::vector<igl::UniformDesc>{
      igl::UniformDesc{
          "modelMatrix", -1, igl::UniformType::Mat4x4, 1, offsetof(VertexFormat, modelMatrix), 0},
      igl::UniformDesc{
          "viewProjectionMatrix",
          -1,
          igl::UniformType::Mat4x4,
          2,
          offsetof(VertexFormat, viewProjectionMatrix),
          sizeof(glm::mat4),
      },
      igl::UniformDesc{
          "scaleZ",
          -1,
          igl::UniformType::Float,
          1,
          offsetof(VertexFormat, scaleZ),
          0,
      }};

  const auto vertUniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, info);
  IGL_ASSERT(vertUniformBuffer->result.isOk());
  *static_cast<VertexFormat*>(vertUniformBuffer->getData()) = vertexParameters_;
  vertUniformBuffer->bind(device, *pipelineState_, *commands);

  commands->bindTexture(textureUnit, BindTarget::kFragment, tex0_.get());
  commands->bindSamplerState(textureUnit, BindTarget::kFragment, samp0_.get());

  commands->bindRenderPipelineState(pipelineState_);

  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
  commands->drawIndexed(3u * 6u * 2u);

  commands->popDebugGroupLabel();
  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_->getColorAttachment(0));
  }

  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers
}

} // namespace igl::shell
