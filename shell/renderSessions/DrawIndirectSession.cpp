/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/DrawIndirectSession.h>

#include "shaders/gen/DrawIndirectSessionAltFragModule.hpp"
#include "shaders/gen/DrawIndirectSessionCompModule.hpp"
#include "shaders/gen/DrawIndirectSessionFragModule.hpp"
#include "shaders/gen/DrawIndirectSessionVertModule.hpp"

#include <chrono>
// @fb-only
#include <shell/shared/platform/Platform.h>
#include <igl/Buffer.h>
#include <igl/Common.h>

namespace {
std::chrono::system_clock::time_point gStartTime;
}

namespace igl::shell {

struct DrawElementsIndirectCommand {
  uint32_t count; // Specifies the number of elements to be rendered.
  uint32_t instanceCount; // Specifies the number of instances of the indexed geometry that
                          // should be drawn.
  uint32_t firstIndex; // Offset to first index (in sizeof(primitive type) units)
  int32_t baseVertex; // Specifies a constant that should be added to each element of indices
                      // when chosing elements from the enabled vertex arrays.
  uint32_t reservedMustBeZero;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
void DrawIndirectSession::initialize() noexcept {
  // Create commandQueue
  auto& device = getPlatform().getDevice();
  commandQueue_ = device.createCommandQueue({}, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  // Initialize render pass
  renderPass_ = {
      .colorAttachments = {{
          .loadAction = igl::LoadAction::Clear,
          .storeAction = igl::StoreAction::Store,
          .clearColor = getPreferredClearColor(),
      }},
  };

  // Initialize buffers.
  Result ret;

  {
    const uint32_t triangleIndices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    const BufferDesc bufDesc{
        .type = BufferDesc::BufferTypeBits::Index,
        .data = triangleIndices,
        .length = sizeof(triangleIndices),
    };
    indexBuffer_ = device.createBuffer(bufDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
  }

  {
    std::vector<DrawElementsIndirectCommand> indirectCommands;

    DrawElementsIndirectCommand command{};
    command.count = 3; // 3 vertices
    command.instanceCount = 1; // 1 instance
    command.firstIndex = 0;
    command.baseVertex = 0;
    command.reservedMustBeZero = 0;
    indirectCommands.push_back(command);

    command.firstIndex = 3;
    indirectCommands.push_back(command);

    command.firstIndex = 6;
    indirectCommands.push_back(command);

    const BufferDesc bufDesc{
        .type = BufferDesc::BufferTypeBits::Storage,
        .data = indirectCommands.data(),
        .length = indirectCommands.size() * sizeof(DrawElementsIndirectCommand),
    };
    indirectBuffer_ = device.createBuffer(bufDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());

    indirectBufferForCompute_ = device.createBuffer(bufDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
  }

  {
    float vertexData[] = {
        -0.5f, 0.0f,  0.0f, 0.5f, 0.0f,  0.0f, 0.0f,  1.0f, 0.0f,

        -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, -0.5f, 0.0f, 0.0f,

        0.0f,  -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.5f,  0.0f, 0.0f,
    };
    const BufferDesc vb0Desc = BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                          .data = vertexData,
                                          .length = sizeof(vertexData)};
    vertexBuffer_ = device.createBuffer(vb0Desc, nullptr);
  }

  // Initialize pipeline state objects.
  {
    const VertexInputStateDesc inputDesc = {
        .numAttributes = 1,
        .attributes =
            {
                {
                    .bufferIndex = 0,
                    .format = VertexAttributeFormat::Float3,
                    .offset = 0,
                    .name = "position",
                    .location = 0,
                },
            },
        .numInputBindings = 1,
        .inputBindings =
            {
                {
                    .stride = sizeof(float) * 3,
                },
            },
    };
    vertexState_ = device.createVertexInputState(inputDesc, nullptr);
  }

  // Initialize shaders.
  {
    shaderStages_ = iglu::spark_sl_compiler::compileShaderStages(
        device,
        getShaderSourceFactory()->makeFromModules(
            {igl::shell::shaders::getDrawIndirectSessionVertModuleDescription(),
             igl::shell::shaders::getDrawIndirectSessionFragModuleDescription()}),
        false /* autoReorderModules */,
        spark_sl::backend::CapabilityFlags{},
        {},
        "",
        ret);
    IGL_DEBUG_ASSERT(ret.isOk());

    shaderStagesAlt_ = iglu::spark_sl_compiler::compileShaderStages(
        device,
        getShaderSourceFactory()->makeFromModules(
            {igl::shell::shaders::getDrawIndirectSessionVertModuleDescription(),
             igl::shell::shaders::getDrawIndirectSessionAltFragModuleDescription()}),
        false /* autoReorderModules */,
        spark_sl::backend::CapabilityFlags{},
        {},
        "",
        ret);
    IGL_DEBUG_ASSERT(ret.isOk());
  }

  // Initialize buffers and shaders for a compute pipeline.
  {
    // Our compute shader spawns three threads, each one will increment
    // an atomic counter (aliased to an indirect buffer).

    buildIndirectBufferStages_ = iglu::spark_sl_compiler::compileShaderStages(
        device,
        getShaderSourceFactory()->makeFromModules(
            {igl::shell::shaders::getDrawIndirectSessionCompModuleDescription()}),
        false /* autoReorderModules */,
        spark_sl::backend::CapabilityFlags{},
        {},
        "",
        ret);
    IGL_DEBUG_ASSERT(ret.isOk());
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void DrawIndirectSession::update(SurfaceTextures surfaceTextures) noexcept {
  // Per IGL guidelines, surfaceTextures.color may be null on some platforms
  // before the surface is ready (e.g., during window resize on Android/iOS).
  if (!surfaceTextures.color) {
    return;
  }
  auto& device = getPlatform().getDevice();
  Result ret;
  // Create/update framebuffer
  if (framebuffer_ == nullptr) {
    framebuffer_ = device.createFramebuffer(
        FramebufferDesc{.colorAttachments = {{.texture = surfaceTextures.color}}}, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  // Create/submit command buffer
  const auto buffer = commandQueue_->createCommandBuffer({}, &ret);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  IGL_DEBUG_ASSERT(ret.isOk());
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);

  if (pipelineState_ == nullptr) {
    const RenderPipelineDesc graphicsDesc = {
        .vertexInputState = vertexState_,
        .shaderStages = shaderStages_,
        .targetDesc =
            {
                .colorAttachments =
                    {
                        {
                            .textureFormat = surfaceTextures.color->getProperties().format,
                        },
                    },
            },
        .cullMode = igl::CullMode::Back,
        .frontFaceWinding = igl::WindingMode::CounterClockwise,
    };

    pipelineState_ = device.createRenderPipeline(graphicsDesc, nullptr);
  }
  if (pipelineStateAlt_ == nullptr) {
    const RenderPipelineDesc graphicsDesc = {
        .vertexInputState = vertexState_,
        .shaderStages = shaderStagesAlt_,
        .targetDesc =
            {
                .colorAttachments =
                    {
                        {
                            .textureFormat = surfaceTextures.color->getProperties().format,
                        },
                    },
            },
        .cullMode = igl::CullMode::Back,
        .frontFaceWinding = igl::WindingMode::CounterClockwise,
    };

    pipelineStateAlt_ = device.createRenderPipeline(graphicsDesc, nullptr);
  }
  if (computePipelineState_ == nullptr) {
    computePipelineState_ = device.createComputePipeline(
        {
            .buffersMap = {{0, genNameHandle("indexCountBuffer")}},
            .shaderStages = buildIndirectBufferStages_,
        },
        nullptr);
    IGL_DEBUG_ASSERT(computePipelineState_ != nullptr);
  }

  auto now = std::chrono::system_clock::now();
  if (gStartTime == std::chrono::system_clock::time_point{}) {
    gStartTime = now;
  }
  if (now < gStartTime + std::chrono::seconds(4)) {
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindVertexBuffer(0, *vertexBuffer_);
    commands->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt32);
    commands->drawIndexed(3);
  } else if (gStartTime + std::chrono::seconds(4) <= now &&
             now <= gStartTime + std::chrono::seconds(8)) {
    commands->bindRenderPipelineState(pipelineStateAlt_);
    commands->bindVertexBuffer(0, *vertexBuffer_);
    commands->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt32);
    commands->multiDrawIndexedIndirect(*indirectBuffer_, 0);
  } else if (gStartTime + std::chrono::seconds(8) <= now &&
             now <= gStartTime + std::chrono::seconds(12)) {
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindVertexBuffer(0, *vertexBuffer_);
    commands->multiDrawIndirect(*indirectBuffer_,
                                0, // indirectBufferOffset
                                3, // drawCount
                                sizeof(DrawElementsIndirectCommand) // stride
    );
  } else if (gStartTime + std::chrono::seconds(12) <= now &&
             now <= gStartTime + std::chrono::seconds(16)) {
    commands->bindRenderPipelineState(pipelineStateAlt_);
    commands->bindVertexBuffer(0, *vertexBuffer_);
    commands->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt32);
    commands->multiDrawIndexedIndirect(*indirectBuffer_,
                                       0, // indirectBufferOffset
                                       3, // drawCount
                                       sizeof(DrawElementsIndirectCommand) // stride
    );
  } else {
    // Create compute command, reset indirect buffer, bind indirect buffer, schedule compute,
    // then maybe barrier (because atomics!), then draw.
    commands->endEncoding(); // call endEncoding before creating another encoder
    commands.reset();
    std::unique_ptr<IComputeCommandEncoder> computeEncoder = buffer->createComputeCommandEncoder();

    {
      DrawElementsIndirectCommand command{};
      command.count = 0; // zero vertices
      command.instanceCount = 1; // 1 instance
      command.firstIndex = 0;
      command.baseVertex = 0;
      command.reservedMustBeZero = 0;
      indirectBufferForCompute_->upload(&command, BufferRange(sizeof(command), 0));
    }
    computeEncoder->bindComputePipelineState(computePipelineState_);
    computeEncoder->bindBuffer(0, indirectBufferForCompute_.get());
    // dispatchThreadGroups arguments should be const&, not & ...
    const igl::Dimensions threadgroupCount(1, 1, 1);
    const igl::Dimensions threadgroupSize(1, 1, 1);
    computeEncoder->dispatchThreadGroups(threadgroupCount, threadgroupSize);
    computeEncoder->endEncoding();
    computeEncoder.reset();

    commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
    IGL_DEBUG_ASSERT(commands != nullptr);
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindVertexBuffer(0, *vertexBuffer_);
    commands->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt32);
    commands->multiDrawIndexedIndirect(*indirectBufferForCompute_, 0);
  }

  commands->endEncoding();

  buffer->present(framebuffer_->getColorAttachment(0));

  commandQueue_->submit(*buffer);
}

} // namespace igl::shell
