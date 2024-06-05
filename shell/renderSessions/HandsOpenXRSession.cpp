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
#include <cstdint>
#include <glm/detail/qualifier.hpp>
#include <glm/gtx/quaternion.hpp>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <shell/renderSessions/HandsOpenXRSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <vector>

namespace igl::shell {

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec4 weight;
  glm::vec4 joint;
};

namespace {

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
                      in vec3 worldNormal;
                      out vec4 fragmentColor;
                      void main() {
                        float att = max(dot(worldNormal, -normalize(vec3(-0.1, -1, 0))), 0.3);
                        fragmentColor = vec4(att, att, att, 1.0);
                      })");
}

std::string getOpenGLVertexShaderSource(igl::IDevice& device) {
  return getProlog(device) + R"(
                      layout(num_views = 2) in;
                      precision highp float;

                      in vec3 position;
                      in vec3 normal;
                      in vec4 weight;
                      in vec4 joint;

                      #define XR_HAND_JOINT_COUNT_EXT 26
                      uniform mat4 jointMatrices[XR_HAND_JOINT_COUNT_EXT];

                      uniform mat4 viewProjectionMatrix[2];

                      out vec3 worldNormal;

                      void main() {
                        vec4 p1 = jointMatrices[int(joint.x)] * vec4(position, 1.0);
                        vec4 p2 = jointMatrices[int(joint.y)] * vec4(position, 1.0);
                        vec4 p3 = jointMatrices[int(joint.z)] * vec4(position, 1.0);
                        vec4 p4 = jointMatrices[int(joint.w)] * vec4(position, 1.0);
                        vec4 worldPos = p1 * weight.x + p2 * weight.y + p3 * weight.z + p4 * weight.w;

                        vec4 n1 = jointMatrices[int(joint.x)] * vec4(normal, 0.0);
                        vec4 n2 = jointMatrices[int(joint.y)] * vec4(normal, 0.0);
                        vec4 n3 = jointMatrices[int(joint.z)] * vec4(normal, 0.0);
                        vec4 n4 = jointMatrices[int(joint.w)] * vec4(normal, 0.0);
                        worldNormal = (n1 * weight.x + n2 * weight.y + n3 * weight.z + n4 * weight.w).xyz;

                        gl_Position = viewProjectionMatrix[gl_ViewID_OVR] * vec4(worldPos.xyz, 1.0);
                      })";
}

const char* getVulkanFragmentShaderSource() {
  return R"(
                      precision highp float;
                      layout(location = 0) in vec3 worldNormal;
                      layout(location = 0) out vec4 fragmentColor;
                      void main() {
                        float att = max(dot(worldNormal, -normalize(vec3(-0.1, -1, 0))), 0.3);
                        fragmentColor = vec4(att, att, att, 1.0);
                      })";
}

const char* getVulkanVertexShaderSource() {
  return R"(
                      #extension GL_OVR_multiview2 : require
                      layout(num_views = 2) in;
                      precision highp float;

                      layout(location = 0) in vec3 position;
                      layout(location = 1) in vec3 normal;
                      layout(location = 2) in vec4 weight;
                      layout(location = 3) in vec4 joint;

                      #define XR_HAND_JOINT_COUNT_EXT 26
                      layout (set = 1, binding = 1, std140) uniform PerFrame {
                        mat4 jointMatrices[XR_HAND_JOINT_COUNT_EXT];
                        mat4 viewProjectionMatrix[2];
                      } perFrame;

                      layout(location = 0) out vec3 worldNormal;

                      void main() {
                        vec4 p1 = perFrame.jointMatrices[int(joint.x)] * vec4(position, 1.0);
                        vec4 p2 = perFrame.jointMatrices[int(joint.y)] * vec4(position, 1.0);
                        vec4 p3 = perFrame.jointMatrices[int(joint.z)] * vec4(position, 1.0);
                        vec4 p4 = perFrame.jointMatrices[int(joint.w)] * vec4(position, 1.0);
                        vec4 worldPos = p1 * weight.x + p2 * weight.y + p3 * weight.z + p4 * weight.w;

                        vec4 n1 = perFrame.jointMatrices[int(joint.x)] * vec4(normal, 0.0);
                        vec4 n2 = perFrame.jointMatrices[int(joint.y)] * vec4(normal, 0.0);
                        vec4 n3 = perFrame.jointMatrices[int(joint.z)] * vec4(normal, 0.0);
                        vec4 n4 = perFrame.jointMatrices[int(joint.w)] * vec4(normal, 0.0);
                        worldNormal = (n1 * weight.x + n2 * weight.y + n3 * weight.z + n4 * weight.w).xyz;

                        gl_Position = perFrame.viewProjectionMatrix[gl_ViewID_OVR] * vec4(worldPos.xyz, 1.0);
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

inline glm::mat4 poseToMat4(const Pose& pose) noexcept {
  return glm::translate(glm::mat4(1.0), glm::vec3(pose.position)) * glm::toMat4(pose.orientation);
}
} // namespace

void HandsOpenXRSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  if (shellParams().handMeshes[0].vertexCountOutput == 0 &&
      shellParams().handMeshes[1].vertexCountOutput == 0) {
    return;
  }

  const auto& handMeshes = shellParams().handMeshes;
  std::vector<Vertex> vertexData;
  std::vector<uint16_t> indices;
  vertexData.reserve(handMeshes[0].vertexCountOutput + handMeshes[1].vertexCountOutput);
  indices.reserve(handMeshes[0].indexCountOutput + handMeshes[1].indexCountOutput);
  for (uint8_t i = 0; i < 2; ++i) {
    handsDrawParams_[i].indexCount = handMeshes[i].indexCountOutput;
    handsDrawParams_[i].indexBufferOffset = indices.size() * sizeof(uint16_t);
    const uint16_t baseVertex = static_cast<uint16_t>(vertexData.size());

    for (size_t j = 0; j < handMeshes[i].vertexCountOutput; ++j) {
      Vertex v;
      v.position = handMeshes[i].vertexPositions[j];
      v.normal = handMeshes[i].vertexNormals[j];
      v.weight = handMeshes[i].vertexBlendWeights[j];
      v.joint = handMeshes[i].vertexBlendIndices[j];
      vertexData.push_back(v);
    }
    for (size_t j = 0; j < handMeshes[i].indexCountOutput; ++j) {
      indices.push_back(baseVertex + static_cast<uint16_t>(handMeshes[i].indices[j]));
    }

    for (size_t j = 0; j < handMeshes[i].jointBindPoses.size(); ++j) {
      jointInvBindMatrix_[i][j] = glm::inverse(poseToMat4(handMeshes[i].jointBindPoses[j]));
    }
  }

  const BufferDesc vb0Desc = BufferDesc(
      BufferDesc::BufferTypeBits::Vertex, vertexData.data(), sizeof(Vertex) * vertexData.size());
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc ibDesc = BufferDesc(
      BufferDesc::BufferTypeBits::Index, indices.data(), sizeof(uint16_t) * indices.size());
  ib0_ = device.createBuffer(ibDesc, nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 4;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[0].offset = offsetof(Vertex, position);
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.attributes[1].format = VertexAttributeFormat::Float3;
  inputDesc.attributes[1].offset = offsetof(Vertex, normal);
  inputDesc.attributes[1].bufferIndex = 0;
  inputDesc.attributes[1].name = "normal";
  inputDesc.attributes[1].location = 1;
  inputDesc.attributes[2].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[2].offset = offsetof(Vertex, weight);
  inputDesc.attributes[2].bufferIndex = 0;
  inputDesc.attributes[2].name = "weight";
  inputDesc.attributes[2].location = 2;
  inputDesc.attributes[3].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[3].offset = offsetof(Vertex, joint);
  inputDesc.attributes[3].bufferIndex = 0;
  inputDesc.attributes[3].name = "joint";
  inputDesc.attributes[3].location = 3;
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(Vertex);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);

  shaderStages_ = getShaderStagesForBackend(device);

  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);

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

void HandsOpenXRSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  if (shellParams().handMeshes[0].vertexCountOutput == 0 &&
      shellParams().handMeshes[1].vertexCountOutput == 0) {
    return;
  }

  // Update uniforms.
  for (size_t i = 0; i < std::min(shellParams().viewParams.size(), size_t(2)); ++i) {
    ub_.viewProjectionMatrix[i] =
        perspectiveAsymmetricFovRH(shellParams().viewParams[i].fov, 0.1f, 100.0f) *
        shellParams().viewParams[i].viewMatrix;
  }

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

  if (pipelineState_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
  }

  if (depthStencilState_ == nullptr) {
    DepthStencilStateDesc depthStencilDesc;
    depthStencilDesc.isDepthWriteEnabled = true;
    depthStencilDesc.compareFunction = CompareFunction::LessEqual;
    depthStencilState_ =
        getPlatform().getDevice().createDepthStencilState(depthStencilDesc, nullptr);
  }

  // Command buffers (1-N per thread): create, submit and forget
  auto buffer = commandQueue_->createCommandBuffer(CommandBufferDesc{}, nullptr);
  const std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  commands->pushDebugGroupLabel("HandsOpenXRSession Commands", igl::Color(0.0f, 1.0f, 0.0f));

  commands->bindVertexBuffer(0, *vb0_);

  iglu::ManagedUniformBufferInfo info;
  info.index = 1;
  info.length = sizeof(UniformBlock);
  info.uniforms = std::vector<igl::UniformDesc>{
      igl::UniformDesc{
          "jointMatrices",
          -1,
          igl::UniformType::Mat4x4,
          kMaxJoints,
          offsetof(UniformBlock, jointMatrices),
          sizeof(glm::mat4),
      },
      igl::UniformDesc{"viewProjectionMatrix",
                       -1,
                       igl::UniformType::Mat4x4,
                       2,
                       offsetof(UniformBlock, viewProjectionMatrix),
                       sizeof(glm::mat4)},
  };

  commands->bindRenderPipelineState(pipelineState_);
  commands->bindDepthStencilState(depthStencilState_);

  for (int i = 0; i < 2; ++i) {
    auto& handTracking = shellParams().handTracking[i];
    IGL_ASSERT(handTracking.jointPose.size() <= kMaxJoints);
    for (size_t j = 0; j < handTracking.jointPose.size(); ++j) {
      ub_.jointMatrices[j] = poseToMat4(handTracking.jointPose[j]) * jointInvBindMatrix_[i][j];
    }

    const auto uniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, info);
    IGL_ASSERT(uniformBuffer->result.isOk());
    *static_cast<UniformBlock*>(uniformBuffer->getData()) = ub_;

    uniformBuffer->bind(device, *pipelineState_, *commands);

    commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16, handsDrawParams_[i].indexBufferOffset);
    commands->drawIndexed(handsDrawParams_[i].indexCount);
  }

  commands->popDebugGroupLabel();
  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_->getColorAttachment(0));
  }

  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers
}

} // namespace igl::shell
