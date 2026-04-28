/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/ClothSimulationSession.h>

#include <IGLU/simdtypes/SimdTypes.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vector>
#include <shell/renderSessions/shaderCode/generated/ClothRenderFragShaderProvider.h>
#include <shell/renderSessions/shaderCode/generated/ClothRenderVertShaderProvider.h>
#include <shell/renderSessions/shaderCode/generated/ObstacleRenderFragShaderProvider.h>
#include <shell/renderSessions/shaderCode/generated/ObstacleRenderVertShaderProvider.h>
#include <shell/renderSessions/shaderCode/generated/UpdateClothNormalCompShaderProvider.h>
#include <shell/renderSessions/shaderCode/generated/UpdateClothPositionCompShaderProvider.h>
#include <shell/renderSessions/shaderCode/generated/UpdateClothVelocityCompShaderProvider.h>
#include <shell/shared/renderSession/ShaderStagesCreator.h>
#include <igl/Buffer.h>
#include <igl/NameHandle.h>

namespace igl::shell {

namespace {
constexpr const uint32_t kN = 128;
constexpr const uint32_t kNumVertices = kN * kN;
constexpr const uint32_t kNumTriangles = (kN - 1) * (kN - 1) * 2;

struct ClothVertex {
  iglu::simdtypes::float3 position; // SIMD 128b aligned
  iglu::simdtypes::float3 velocity; // SIMD 128b aligned
  iglu::simdtypes::float3 normal; // SIMD 128b aligned
};

struct ObstacleVertex {
  iglu::simdtypes::float2 position;
};

std::vector<ClothVertex> getClothVertexData() {
  std::vector<ClothVertex> vertices(static_cast<size_t>(kN * kN));
  const float cellSize = 1.0f / (float)kN;
  for (int i = 0; i < kN; ++i) {
    for (int j = 0; j < kN; ++j) {
      const int index = i * kN + j;
      vertices[index].velocity = {0.0, 0.0, 0.0};
      vertices[index].normal = {0.0, 0.0, 0.0};
      vertices[index].position = {
          (kN - j) * cellSize,
          (kN - i) * cellSize / std::sqrt(2.0f),
          i * cellSize / std::sqrt(2.0f),
      };
    }
  }
  return vertices;
}

std::vector<uint32_t> getClothIndexData() {
  std::vector<uint32_t> indices(kNumTriangles);
  for (int i = 0; i < kN - 1; ++i) {
    for (int j = 0; j < kN - 1; ++j) {
      const int squareIndex = i * (kN - 1) + j;
      indices[squareIndex * 6 + 0] = i * kN + j;
      indices[squareIndex * 6 + 1] = (i + 1) * kN + j;
      indices[squareIndex * 6 + 2] = i * kN + (j + 1);
      // 2nd triangle of the square
      indices[squareIndex * 6 + 3] = (i + 1) * kN + j + 1;
      indices[squareIndex * 6 + 4] = i * kN + (j + 1);
      indices[squareIndex * 6 + 5] = (i + 1) * kN + j;
    }
  }
  return indices;
}

std::vector<ObstacleVertex> getObstacleVertexData() {
  return {{{-1.0f, 1.0f}}, {{1.0f, 1.0f}}, {{-1.0f, -1.0f}}, {{1.0f, -1.0f}}};
}

std::vector<uint32_t> getObstacleIndexData() {
  return {
      0,
      1,
      2,
      1,
      3,
      2,
  };
}

struct UBO {
  float ballRadius{};
  float fov{};
  float aspectRatio{};

  int n{};
  float cellSize{};
  float gravity = 0.5;
  float stiffness = 1600.0;
  float damping = 2.0;
  float dt = 5e-4;

  iglu::simdtypes::float3 eye{};
  iglu::simdtypes::float3 ballCenter{};
  iglu::simdtypes::float3 lightPos{};

  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 viewProjection;
};

UBO getUniformBuffer(float aspectRatio) {
  UBO ubo;
  ubo.eye = {0.5, -0.5, 2.0};
  ubo.ballCenter = {0.5, -0.5, 0.0};
  ubo.lightPos = {0.5, 1.0, 2.0};
  ubo.ballRadius = 0.2;
  ubo.fov = M_PI / 4;
  ubo.aspectRatio = aspectRatio;

  ubo.n = kN;
  ubo.cellSize = 1.f / (float)kN;

  ubo.view = glm::lookAt(glm::vec3(ubo.eye.x, ubo.eye.y, ubo.eye.z),
                         glm::vec3(ubo.ballCenter.x, ubo.ballCenter.y, ubo.ballCenter.z),
                         glm::vec3(0.0, 1.0, 0.0));
  ubo.projection = glm::perspective(ubo.fov, ubo.aspectRatio, 0.1f, 10.f);
  ubo.viewProjection = ubo.projection * ubo.view;
  return ubo;
}

} // namespace

static bool isDeviceCompatible(IDevice& device) noexcept {
  const auto backendtype = device.getBackendType();
  if (backendtype == BackendType::OpenGL) {
    return device.hasFeature(DeviceFeatures::Compute);
  } else if (backendtype == BackendType::Vulkan) {
    return false;
  } else if (backendtype == BackendType::Metal) {
    return true;
  }

  return false;
}

void ClothSimulationSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  auto clothVertexData = getClothVertexData();
  const BufferDesc clothVertexBufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                         .data = clothVertexData.data(),
                                         .length = kNumVertices * sizeof(ClothVertex)};
  clothVertexBuffer_ = device.createBuffer(clothVertexBufferDesc, nullptr);
  IGL_DEBUG_ASSERT(clothVertexBuffer_ != nullptr);

  auto clothIndexData = getClothIndexData();
  const BufferDesc clothIndexBufferDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = clothIndexData.data(),
      .length = static_cast<size_t>(kNumTriangles * 3) * sizeof(uint32_t)};
  clothIndexBuffer_ = device.createBuffer(clothIndexBufferDesc, nullptr);
  IGL_DEBUG_ASSERT(clothIndexBuffer_ != nullptr);

  auto obstacleVertexData = getObstacleVertexData();
  const BufferDesc obstacleVertexBufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                            .data = obstacleVertexData.data(),
                                            .length = 4 * sizeof(ObstacleVertex)};
  obstacleVertexBuffer_ = device.createBuffer(obstacleVertexBufferDesc, nullptr);
  IGL_DEBUG_ASSERT(obstacleVertexBuffer_ != nullptr);

  auto obstacleIndexData = getObstacleIndexData();
  const BufferDesc obstacleIndexBufferDesc{.type = BufferDesc::BufferTypeBits::Storage,
                                           .data = obstacleIndexData.data(),
                                           .length = static_cast<size_t>(2 * 3) * sizeof(uint32_t)};
  obstacleIndexBuffer_ = device.createBuffer(obstacleIndexBufferDesc, nullptr);
  IGL_DEBUG_ASSERT(obstacleIndexBuffer_ != nullptr);

  const VertexInputStateDesc clothVertexInputDesc = {
      .numAttributes = 2,
      .attributes =
          {
              VertexAttribute{.bufferIndex = 0,
                              .format = VertexAttributeFormat::Float3,
                              .offset = offsetof(ClothVertex, position),
                              .name = "position",
                              .location = 0},
              VertexAttribute{.bufferIndex = 0,
                              .format = VertexAttributeFormat::Float3,
                              .offset = offsetof(ClothVertex, normal),
                              .name = "normal",
                              .location = 1},
          },
      .numInputBindings = 1,
      .inputBindings = {{.stride = sizeof(ClothVertex)}},
  };
  clothVertexInput_ = device.createVertexInputState(clothVertexInputDesc, nullptr);
  IGL_DEBUG_ASSERT(clothVertexInput_ != nullptr);

  const VertexInputStateDesc obstacleVertexInputDesc = {
      .numAttributes = 1,
      .attributes =
          {
              VertexAttribute{.bufferIndex = 0,
                              .format = VertexAttributeFormat::Float2,
                              .offset = offsetof(ObstacleVertex, position),
                              .name = "position",
                              .location = 0},
          },
      .numInputBindings = 1,
      .inputBindings = {{.stride = sizeof(ObstacleVertex)}},
  };
  obstacleVertexInput_ = device.createVertexInputState(obstacleVertexInputDesc, nullptr);
  IGL_DEBUG_ASSERT(obstacleVertexInput_ != nullptr);

  const igl::Result result;

  {
    auto vertProvider = ClothRenderVertShaderProvider();
    auto fragProvider = ClothRenderFragShaderProvider();
    clothShaderStages_ =
        createRenderPipelineStages(getPlatform().getDevice(), vertProvider, fragProvider);
    IGL_DEBUG_ASSERT(clothShaderStages_ != nullptr);
  }
  {
    auto vertProvider = ObstacleRenderVertShaderProvider();
    auto fragProvider = ObstacleRenderFragShaderProvider();

    obstacleShaderStages_ =
        createRenderPipelineStages(getPlatform().getDevice(), vertProvider, fragProvider);
    IGL_DEBUG_ASSERT(obstacleShaderStages_ != nullptr);
  }

  {
    updatePositionStages_ = createComputePipelineStages(getPlatform().getDevice(),
                                                        UpdateClothPositionCompShaderProvider());
    IGL_DEBUG_ASSERT(updatePositionStages_ != nullptr);
  }
  {
    updateVelocityStages_ = createComputePipelineStages(getPlatform().getDevice(),
                                                        UpdateClothVelocityCompShaderProvider());
    IGL_DEBUG_ASSERT(updateVelocityStages_ != nullptr);
  }
  {
    updateNormalStages_ = createComputePipelineStages(getPlatform().getDevice(),
                                                      UpdateClothNormalCompShaderProvider());
    IGL_DEBUG_ASSERT(updateNormalStages_ != nullptr);
  }

  // Command queue: backed by different types of GPU HW queues
  commandQueue_ = device.createCommandQueue(CommandQueueDesc{}, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  depthStencilState_ = device.createDepthStencilState(
      DepthStencilStateDesc{
          .compareFunction = CompareFunction::Less,
          .isDepthWriteEnabled = true,
      },
      nullptr);
}

void ClothSimulationSession::createOrUpdateDefaultFramebuffer(
    const igl::SurfaceTextures& surfaceTextures) {
  if (framebuffer_) {
    framebuffer_->updateDrawable(surfaceTextures.color);
    return;
  }

  // create framebuffer
  framebuffer_ = getPlatform().getDevice().createFramebuffer(
      FramebufferDesc{
          .colorAttachments = {{.texture = surfaceTextures.color}},
          .depthAttachment = {.texture = surfaceTextures.depth},
      },
      nullptr);

  renderPass_ = {
      .colorAttachments =
          {
              {
                  .loadAction = LoadAction::Clear,
                  .storeAction = StoreAction::Store,
                  .clearColor = getPreferredClearColor(),
              },
          },
      .depthAttachment =
          {
              .loadAction = LoadAction::Clear,
              .clearDepth = 1.0,
          },
  };

  IGL_DEBUG_ASSERT(framebuffer_ != nullptr);

  const auto aspectRatio = surfaceTextures.color->getAspectRatio();
  auto ubo = getUniformBuffer(aspectRatio);
  if (!uniformBuffer_) {
    uniformBuffer_ = getPlatform().getDevice().createBuffer(
        BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform, .length = sizeof(UBO)}, nullptr);
    IGL_DEBUG_ASSERT(uniformBuffer_ != nullptr);
  }
  uniformBuffer_->upload(&ubo, BufferRange(sizeof(ubo), 0));
}

void ClothSimulationSession::update(SurfaceTextures surfaceTextures) noexcept {
  auto& device = getPlatform().getDevice();
  if (!isDeviceCompatible(device)) {
    return;
  }

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  const CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  if (!buffer) {
    return;
  }

  if (updatePositionPipelineState_ == nullptr) {
    updatePositionPipelineState_ = device.createComputePipeline(
        ComputePipelineDesc{
            .shaderStages = updatePositionStages_,
        },
        nullptr);
    IGL_DEBUG_ASSERT(updatePositionPipelineState_ != nullptr);
  }

  if (updateVelocityPipelineState_ == nullptr) {
    updateVelocityPipelineState_ = device.createComputePipeline(
        ComputePipelineDesc{
            .shaderStages = updateVelocityStages_,
        },
        nullptr);
    IGL_DEBUG_ASSERT(updateVelocityPipelineState_ != nullptr);
  }

  if (updateNormalPipelineState_ == nullptr) {
    updateNormalPipelineState_ = device.createComputePipeline(
        ComputePipelineDesc{
            .shaderStages = updateNormalStages_,
        },
        nullptr);
    IGL_DEBUG_ASSERT(updateNormalPipelineState_ != nullptr);
  }

  auto computeEncoder0 = buffer->createComputeCommandEncoder();
  IGL_DEBUG_ASSERT(computeEncoder0 != nullptr);

  // display result of compute pass
  createOrUpdateDefaultFramebuffer(surfaceTextures);

  if (computeEncoder0) {
    const Dimensions threadgroupSize(16, 16, 1);
    const Dimensions threadgroupCount(kN / 16, kN / 16, 1);

    computeEncoder0->bindBuffer(0, clothVertexBuffer_.get());
    computeEncoder0->bindBuffer(1, uniformBuffer_.get());

    for (int i = 0; i < 30; ++i) {
      computeEncoder0->bindComputePipelineState(updateVelocityPipelineState_);
      computeEncoder0->dispatchThreadGroups(threadgroupCount, threadgroupSize);

      computeEncoder0->bindComputePipelineState(updatePositionPipelineState_);
      computeEncoder0->dispatchThreadGroups(threadgroupCount, threadgroupSize);
    }

    computeEncoder0->bindComputePipelineState(updateNormalPipelineState_);
    computeEncoder0->dispatchThreadGroups(threadgroupCount, threadgroupSize);

    computeEncoder0->endEncoding();
  }

  if (clothRenderPipelineState_ == nullptr) {
    const RenderPipelineDesc graphicsDesc = {
        .vertexInputState = clothVertexInput_,
        .shaderStages = clothShaderStages_,
        .targetDesc =
            {
                .colorAttachments = {{.textureFormat =
                                          surfaceTextures.color->getProperties().format}},
                .depthAttachmentFormat = surfaceTextures.depth->getProperties().format,
            },
        .cullMode = igl::CullMode::Disabled,
        .frontFaceWinding = igl::WindingMode::Clockwise,
    };

    clothRenderPipelineState_ = device.createRenderPipeline(graphicsDesc, nullptr);
    IGL_DEBUG_ASSERT(clothRenderPipelineState_ != nullptr);
  }

  if (obstacleRenderPipelineState_ == nullptr) {
    const RenderPipelineDesc graphicsDesc = {
        .vertexInputState = obstacleVertexInput_,
        .shaderStages = obstacleShaderStages_,
        .targetDesc =
            {
                .colorAttachments = {{.textureFormat =
                                          surfaceTextures.color->getProperties().format}},
                .depthAttachmentFormat = surfaceTextures.depth->getProperties().format,
            },
        .cullMode = igl::CullMode::Disabled,
        .frontFaceWinding = igl::WindingMode::Clockwise,
    };

    obstacleRenderPipelineState_ = device.createRenderPipeline(graphicsDesc, nullptr);
    IGL_DEBUG_ASSERT(obstacleRenderPipelineState_ != nullptr);
  }

  auto renderEncoder = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(renderEncoder != nullptr);
  if (renderEncoder) {
    renderEncoder->bindDepthStencilState(depthStencilState_);

    renderEncoder->bindVertexBuffer(0, *clothVertexBuffer_);
    renderEncoder->bindIndexBuffer(*clothIndexBuffer_, IndexFormat::UInt32);
    renderEncoder->bindBuffer(1, uniformBuffer_.get());
    renderEncoder->bindRenderPipelineState(clothRenderPipelineState_);
    renderEncoder->drawIndexed(static_cast<size_t>(kNumTriangles * 3));

    renderEncoder->bindVertexBuffer(0, *obstacleVertexBuffer_);
    renderEncoder->bindIndexBuffer(*obstacleIndexBuffer_, IndexFormat::UInt32);
    renderEncoder->bindBuffer(1, uniformBuffer_.get());
    renderEncoder->bindRenderPipelineState(obstacleRenderPipelineState_);
    renderEncoder->drawIndexed(6);

    renderEncoder->endEncoding();
  }
  buffer->present(surfaceTextures.color);

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer); // Guarantees ordering between command buffers
}

#undef N

} // namespace igl::shell
