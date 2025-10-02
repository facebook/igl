/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <shell/renderSessions/ThreeCubesRenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Version.h>

#include <cstddef>

namespace igl::shell {

namespace {

// Cube vertex data (position + color)
const float kHalf = 1.0f;
VertexPosColor vertexData[] = {
    // Front face (red tint)
    {{-kHalf, kHalf, -kHalf}, {1.0, 0.3, 0.3}},
    {{kHalf, kHalf, -kHalf}, {1.0, 0.3, 0.3}},
    {{-kHalf, -kHalf, -kHalf}, {0.8, 0.2, 0.2}},
    {{kHalf, -kHalf, -kHalf}, {0.8, 0.2, 0.2}},
    // Back face (blue tint)
    {{kHalf, kHalf, kHalf}, {0.3, 0.3, 1.0}},
    {{-kHalf, kHalf, kHalf}, {0.3, 0.3, 1.0}},
    {{kHalf, -kHalf, kHalf}, {0.2, 0.2, 0.8}},
    {{-kHalf, -kHalf, kHalf}, {0.2, 0.2, 0.8}},
};

uint16_t indexData[] = {
    0, 1, 2, 1, 3, 2, // front
    1, 4, 3, 4, 6, 3, // right
    4, 5, 6, 5, 7, 6, // back
    5, 0, 7, 0, 2, 7, // left
    5, 4, 0, 4, 1, 0, // top
    2, 3, 7, 3, 6, 7  // bottom
};

std::string getProlog(IDevice& device) {
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
            float3 color [[attribute(1)]];
          };

          struct VertexOut {
            float4 position [[position]];
            float3 color;
          };

          vertex VertexOut vertexShader(VertexIn in [[stage_in]],
                 constant VertexUniformBlock &vUniform[[buffer(1)]]) {
            VertexOut out;
            out.position = vUniform.mvpMatrix * float4(in.position, 1.0);
            out.color = in.color;
            return out;
           }

           fragment float4 fragmentShader(VertexOut in[[stage_in]]) {
             return float4(in.color, 1.0);
           }
        )";
}

std::string getOpenGLFragmentShaderSource(IDevice& device) {
  return getProlog(device) + std::string(R"(
                      precision highp float;
                      in vec3 color;
                      out vec4 fragmentColor;
                      void main() {
                        fragmentColor = vec4(color, 1.0);
                      })");
}

std::string getOpenGLVertexShaderSource(IDevice& device) {
  return getProlog(device) + R"(
                      precision highp float;
                      uniform mat4 mvpMatrix;
                      in vec3 position;
                      in vec3 color_in;
                      out vec3 color;

                      void main() {
                        gl_Position = mvpMatrix * vec4(position, 1.0);
                        color = color_in;
                      })";
}

const char* getVulkanFragmentShaderSource() {
  return R"(
                      precision highp float;
                      layout(location = 0) in vec3 color;
                      layout(location = 0) out vec4 out_FragColor;

                      void main() {
                        out_FragColor = vec4(color, 1.0);
                      })";
}

const char* getVulkanVertexShaderSource() {
  return R"(
                      precision highp float;

                      layout (set = 1, binding = 1, std140) uniform PerFrame {
                        mat4 mvpMatrix;
                      } perFrame;

                      layout(location = 0) in vec3 position;
                      layout(location = 1) in vec3 color_in;
                      layout(location = 0) out vec3 color;

                      void main() {
                        gl_Position = perFrame.mvpMatrix * vec4(position, 1.0);
                        color = color_in;
                      })";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
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
  case igl::BackendType::Custom:
    IGL_DEBUG_ABORT("IGLSamples not set up for Custom");
    return nullptr;
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

void ThreeCubesRenderSession::initializeCubeTransforms() {
  // Cube 1: Left, rotating on Y axis
  cubes_[0].position = glm::vec3(-3.0f, 0.0f, 0.0f);
  cubes_[0].rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
  cubes_[0].rotationSpeed = 1.0f;
  cubes_[0].currentAngle = 0.0f;
  cubes_[0].color = glm::vec3(1.0f, 0.3f, 0.3f); // Red

  // Cube 2: Center, rotating on diagonal axis
  cubes_[1].position = glm::vec3(0.0f, 0.0f, 0.0f);
  cubes_[1].rotationAxis = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
  cubes_[1].rotationSpeed = 1.5f;
  cubes_[1].currentAngle = 0.0f;
  cubes_[1].color = glm::vec3(0.3f, 1.0f, 0.3f); // Green

  // Cube 3: Right, rotating on XZ diagonal
  cubes_[2].position = glm::vec3(3.0f, 0.0f, 0.0f);
  cubes_[2].rotationAxis = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));
  cubes_[2].rotationSpeed = 0.75f;
  cubes_[2].currentAngle = 0.0f;
  cubes_[2].color = glm::vec3(0.3f, 0.3f, 1.0f); // Blue
}

void ThreeCubesRenderSession::updateCubeTransform(size_t index, float deltaTime) {
  cubes_[index].currentAngle += cubes_[index].rotationSpeed * deltaTime;
}

void ThreeCubesRenderSession::setVertexParams(size_t cubeIndex, float aspectRatio) {
  // Perspective projection
  const float fov = 45.0f * (M_PI / 180.0f);
  const glm::mat4 projectionMat = glm::perspectiveLH(fov, aspectRatio, 0.1f, 100.0f);

  // Model transform for this cube
  const auto& cube = cubes_[cubeIndex];
  const glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), cube.position + glm::vec3(0.0f, 0.0f, 8.0f)) *
                              glm::rotate(glm::mat4(1.0f), cube.currentAngle, cube.rotationAxis);

  // Combined MVP matrix
  vertexUniforms_.mvpMatrix = projectionMat * modelMat;
}

void ThreeCubesRenderSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Initialize cube transforms
  initializeCubeTransforms();

  // Vertex buffer, Index buffer and Vertex Input
  const BufferDesc vb0Desc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
  vb0_ = device.createBuffer(vb0Desc, nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, nullptr);

  const VertexInputStateDesc inputDesc = {
      .numAttributes = 2,
      .attributes = {{
                         .bufferIndex = 0,
                         .format = VertexAttributeFormat::Float3,
                         .offset = offsetof(VertexPosColor, position),
                         .name = "position",
                         .location = 0,
                     },
                     {
                         .bufferIndex = 0,
                         .format = VertexAttributeFormat::Float3,
                         .offset = offsetof(VertexPosColor, color),
                         .name = "color_in",
                         .location = 1,
                     }},
      .numInputBindings = 1,
      .inputBindings = {{.stride = sizeof(VertexPosColor)}},
  };
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);

  shaderStages_ = getShaderStagesForBackend(device);

  // Command queue
  commandQueue_ = device.createCommandQueue({}, nullptr);

  renderPass_ = {
      .colorAttachments = {{
          .loadAction = LoadAction::Clear,
          .storeAction = StoreAction::Store,
          .clearColor = getPreferredClearColor(),
      }},
      .depthAttachment = {.loadAction = LoadAction::Clear, .clearDepth = 1.0},
  };
}

void ThreeCubesRenderSession::update(SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();

  // Update cube rotations
  const float deltaTime = getDeltaSeconds();
  for (size_t i = 0; i < 3; ++i) {
    updateCubeTransform(i, deltaTime);
  }

  Result ret;
  if (framebuffer_ == nullptr) {
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;

    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  if (pipelineState_ == nullptr) {
    // Graphics pipeline
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    graphicsDesc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
  }

  // Command buffer
  auto buffer = commandQueue_->createCommandBuffer({}, nullptr);
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindVertexBuffer(0, *vb0_);
  commands->bindRenderPipelineState(pipelineState_);
  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);

  // Draw each cube with its own transform
  const float aspectRatio = surfaceTextures.color->getAspectRatio();
  for (size_t i = 0; i < 3; ++i) {
    setVertexParams(i, aspectRatio);

    // Bind vertex uniforms
    iglu::ManagedUniformBufferInfo info;
    info.index = 1;
    info.length = sizeof(VertexUniforms);
    info.uniforms = std::vector<UniformDesc>{UniformDesc{
        "mvpMatrix",
        -1,
        igl::UniformType::Mat4x4,
        1,
        offsetof(VertexUniforms, mvpMatrix),
        0,
    }};

    const std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer =
        std::make_shared<iglu::ManagedUniformBuffer>(device, info);
    IGL_DEBUG_ASSERT(vertUniformBuffer->result.isOk());
    *static_cast<VertexUniforms*>(vertUniformBuffer->getData()) = vertexUniforms_;
    vertUniformBuffer->bind(device, *pipelineState_, *commands);

    // Draw this cube
    commands->drawIndexed(static_cast<size_t>(36));
  }

  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(framebuffer_->getColorAttachment(0));
  }

  commandQueue_->submit(*buffer);
}

} // namespace igl::shell
