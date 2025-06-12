/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/shaderCross/ShaderCross.h>
#include <IGLU/shaderCross/ShaderCrossUniformBuffer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/detail/qualifier.hpp>
#include <shell/renderSessions/HelloOpenXRSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/RenderCommandEncoder.h>

namespace igl::shell {

namespace {

struct VertexPosUvw {
  glm::vec3 position;
  glm::vec3 uvw;
};

const float kHalf = 1.0f;
const VertexPosUvw kVertexData0[] = {
    {{-kHalf, kHalf, -kHalf}, {0.0, 1.0, 0.0}},
    {{kHalf, kHalf, -kHalf}, {1.0, 1.0, 0.0}},
    {{-kHalf, -kHalf, -kHalf}, {0.0, 0.0, 0.0}},
    {{kHalf, -kHalf, -kHalf}, {1.0, 0.0, 0.0}},
    {{kHalf, kHalf, kHalf}, {1.0, 1.0, 1.0}},
    {{-kHalf, kHalf, kHalf}, {0.0, 1.0, 1.0}},
    {{kHalf, -kHalf, kHalf}, {1.0, 0.0, 1.0}},
    {{-kHalf, -kHalf, kHalf}, {0.0, 0.0, 1.0}},
};
constexpr uint16_t kIndexData[] = {0, 1, 2, 1, 3, 2, 1, 4, 3, 4, 6, 3, 4, 5, 6, 5, 7, 6,
                                   5, 0, 7, 0, 2, 7, 5, 4, 0, 4, 1, 0, 2, 3, 7, 3, 6, 7};

[[nodiscard]] const char* getVulkanFragmentShaderSource() {
  return R"(#version 450
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

[[nodiscard]] std::string getVertexShaderProlog(bool stereoRendering) {
  return stereoRendering ? R"(#version 450
    #extension GL_OVR_multiview2 : require
    layout(num_views = 2) in;
    precision highp float;

    #define VIEW_ID int(gl_ViewID_OVR)
  )"
                         : R"(#version 450
    precision highp float;

    #define VIEW_ID perFrame.viewId
  )";
}

[[nodiscard]] std::string getVulkanVertexShaderSource(bool stereoRendering) {
  return getVertexShaderProlog(stereoRendering) + R"(
            layout (set = 1, binding = 1, std140) uniform PerFrame {
              mat4 modelMatrix;
              mat4 viewProjectionMatrix[2];
              float scaleZ;
              int viewId;
            } perFrame;

            layout(location = 0) in vec3 position;
            layout(location = 1) in vec3 uvw_in;
            layout(location = 0) out vec3 uvw;
            layout(location = 1) out vec3 color;

            void main() {
              mat4 mvpMatrix = perFrame.viewProjectionMatrix[VIEW_ID] * perFrame.modelMatrix;
              gl_Position = mvpMatrix * vec4(position, 1.0);
              uvw = vec3(uvw_in.x, uvw_in.y, (uvw_in.z - 0.5) * perFrame.scaleZ + 0.5);
              color = vec3(1.0, 1.0, 0.0);
            })";
}

[[nodiscard]] std::unique_ptr<IShaderStages> getShaderStagesForBackend(
    IDevice& device,
    const iglu::ShaderCross& shaderCross,
    bool stereoRendering) noexcept {
  switch (device.getBackendType()) {
  case igl::BackendType::Metal:
    IGL_DEBUG_ABORT("Metal is not supported");
    return nullptr;
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getVulkanVertexShaderSource(stereoRendering).c_str(),
        "main",
        "",
        getVulkanFragmentShaderSource(),
        "main",
        "",
        nullptr);
  case igl::BackendType::OpenGL: {
    Result res;
    const auto vs = shaderCross.crossCompileFromVulkanSource(
        getVulkanVertexShaderSource(stereoRendering).c_str(), igl::ShaderStage::Vertex, &res);
    IGL_DEBUG_ASSERT(res.isOk(), res.message.c_str());

    const auto fs = shaderCross.crossCompileFromVulkanSource(
        getVulkanFragmentShaderSource(), igl::ShaderStage::Fragment, &res);
    IGL_DEBUG_ASSERT(res.isOk(), res.message.c_str());

    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        vs.c_str(),
        shaderCross.entryPointName(igl::ShaderStage::Vertex),
        "",
        fs.c_str(),
        shaderCross.entryPointName(igl::ShaderStage::Fragment),
        "",
        nullptr);
  }
  default:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;
  }
}

[[nodiscard]] bool isDeviceCompatible(IDevice& device) noexcept {
  return device.hasFeature(DeviceFeatures::Multiview);
}

[[nodiscard]] glm::mat4 perspectiveAsymmetricFovRH(const igl::shell::Fov& fov,
                                                   float nearZ,
                                                   float farZ) noexcept {
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
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, kVertexData0, sizeof(kVertexData0));
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, kIndexData, sizeof(kIndexData));
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

  const bool stereoRendering = shellParams().viewParams.size() > 1;

  createSamplerAndTextures(device);
  const iglu::ShaderCross shaderCross(device);
  shaderStages_ = getShaderStagesForBackend(device, shaderCross, stereoRendering);

  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{};
  commandQueue_ = device.createCommandQueue(desc, nullptr);

  // Set up vertex uniform data
  ub_.scaleZ = 1.0f;

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

void HelloOpenXRSession::updateUniformBlock() {
  // rotating animation
  static float angle = 0.0f, scaleZ = 1.0f, ss = 0.005f;
  angle += 0.005f;
  scaleZ += ss;
  scaleZ = scaleZ < 0.0f ? 0.0f : (scaleZ > 1.0 ? 1.0f : scaleZ);
  if (scaleZ <= 0.05f || scaleZ >= 1.0f) {
    ss *= -1.0f;
  }

  const glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)) *
                           glm::rotate(glm::mat4(1.0f), -0.2f, glm::vec3(1.0f, 0.0f, 0.0f));
  ub_.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.f, -8.0f)) * rotMat *
                    glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, scaleZ));
  for (size_t i = 0; i < std::min(shellParams().viewParams.size(), size_t(2)); ++i) {
    const auto viewIndex = shellParams().viewParams[i].viewIndex;

    ub_.viewProjectionMatrix[viewIndex] =
        perspectiveAsymmetricFovRH(shellParams().viewParams[i].fov, 0.1f, 100.0f) *
        shellParams().viewParams[i].viewMatrix;
    ub_.viewId = viewIndex;
  }

  ub_.scaleZ = scaleZ;
}

void HelloOpenXRSession::update(SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  updateUniformBlock();

  IGL_DEBUG_ASSERT(!shellParams().viewParams.empty());
  const auto viewIndex = shellParams().viewParams[0].viewIndex;

  Result ret;
  if (framebuffer_[viewIndex] == nullptr) {
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;

    framebufferDesc.mode = surfaceTextures.color->getNumLayers() > 1 ? FramebufferMode::Stereo
                                                                     : FramebufferMode::Mono;

    framebuffer_[viewIndex] = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_[viewIndex] != nullptr);
  } else {
    framebuffer_[viewIndex]->updateDrawable(surfaceTextures.color);
  }

  constexpr uint32_t textureUnit = 0;
  if (pipelineState_ == nullptr) {
    // Graphics pipeline: state batch that fully configures GPU for rendering

    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_[viewIndex]->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer_[viewIndex]->getDepthAttachment()->getProperties().format;
    graphicsDesc.fragmentUnitSamplerMap[textureUnit] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
  }

  // Command buffers (1-N per thread): create, submit and forget
  auto buffer = commandQueue_->createCommandBuffer(CommandBufferDesc{}, nullptr);
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_[viewIndex]);
  commands->pushDebugGroupLabel("HelloOpenXRSession Commands", Color(0.0f, 1.0f, 0.0f));

  commands->bindVertexBuffer(0, *vb0_);

  iglu::ManagedUniformBufferInfo info;
  info.index = 1;
  info.length = sizeof(UniformBlock);
  info.uniforms = std::vector<UniformDesc>{
      UniformDesc{
          "modelMatrix", -1, igl::UniformType::Mat4x4, 1, offsetof(UniformBlock, modelMatrix), 0},
      UniformDesc{
          "viewProjectionMatrix",
          -1,
          igl::UniformType::Mat4x4,
          2,
          offsetof(UniformBlock, viewProjectionMatrix),
          sizeof(glm::mat4),
      },
      UniformDesc{
          "scaleZ",
          -1,
          igl::UniformType::Float,
          1,
          offsetof(UniformBlock, scaleZ),
          0,
      },
      UniformDesc{
          "viewId",
          -1,
          igl::UniformType::Int,
          1,
          offsetof(UniformBlock, viewId),
          0,
      }};

  const auto ubo = std::make_shared<iglu::ShaderCrossUniformBuffer>(device, "perFrame", info);
  IGL_DEBUG_ASSERT(ubo->result.isOk());
  *static_cast<UniformBlock*>(ubo->getData()) = ub_;

  ubo->bind(device, *pipelineState_, *commands);

  commands->bindTexture(textureUnit, BindTarget::kFragment, tex0_.get());
  commands->bindSamplerState(textureUnit, BindTarget::kFragment, samp0_.get());

  commands->bindRenderPipelineState(pipelineState_);

  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
  commands->drawIndexed(static_cast<size_t>(3u * 6u * 2u));

  commands->popDebugGroupLabel();
  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_[viewIndex]->getColorAttachment(0));
  }

  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers
}

} // namespace igl::shell
