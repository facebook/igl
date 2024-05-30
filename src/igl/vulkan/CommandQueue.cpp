/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/RenderCommandEncoder.h>
#include <igl/vulkan/SyncManager.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace igl {
namespace vulkan {

CommandQueue::CommandQueue(Device& device, const CommandQueueDesc& desc) :
  device_(device), desc_(desc) {
  IGL_ASSERT(desc_.type == CommandQueueType::Graphics || desc_.type == CommandQueueType::Compute);
}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                  Result* outResult) {
  IGL_PROFILER_FUNCTION();

  // for now, we want only 1 command buffer
  IGL_ASSERT(!isInsideFrame_);

  isInsideFrame_ = true;

  return std::make_shared<CommandBuffer>(device_.getVulkanContext(), desc);
}

SubmitHandle CommandQueue::submit(const ICommandBuffer& cmdBuffer, bool /* endOfFrame */) {
  IGL_PROFILER_FUNCTION();
  VulkanContext& ctx = device_.getVulkanContext();

  if (ctx.enhancedShaderDebuggingStore_) {
    ctx.enhancedShaderDebuggingStore_->installBufferBarrier(cmdBuffer);
  }

  incrementDrawCount(cmdBuffer.getCurrentDrawCount());

  IGL_ASSERT(isInsideFrame_);

  auto* vkCmdBuffer =
      const_cast<vulkan::CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&cmdBuffer));
  const bool presentIfNotDebugging = ctx.enhancedShaderDebuggingStore_ == nullptr;
  auto submitHandle = endCommandBuffer(ctx, vkCmdBuffer, presentIfNotDebugging);

  if (ctx.enhancedShaderDebuggingStore_) {
    enhancedShaderDebuggingPass(ctx, vkCmdBuffer);
  }

  return submitHandle;
}

SubmitHandle CommandQueue::endCommandBuffer(const igl::vulkan::VulkanContext& ctx,
                                            igl::vulkan::CommandBuffer* cmdBuffer,
                                            bool present) {
  IGL_PROFILER_FUNCTION();

  const bool isGraphicsQueue = desc_.type == CommandQueueType::Graphics;

  // Submit to the graphics queue.
  const bool shouldPresent = isGraphicsQueue && ctx.hasSwapchain() &&
                             cmdBuffer->isFromSwapchain() && present;
  if (shouldPresent) {
    ctx.immediate_->waitSemaphore(ctx.swapchain_->acquireSemaphore_->vkSemaphore_);
  }

  cmdBuffer->lastSubmitHandle_ = ctx.immediate_->submit(cmdBuffer->wrapper_);

  if (shouldPresent) {
    ctx.present();
  }
  ctx.markSubmitted(cmdBuffer->lastSubmitHandle_);
  ctx.syncManager_->markSubmitted(cmdBuffer->lastSubmitHandle_);
  ctx.processDeferredTasks();
  ctx.stagingDevice_->mergeRegionsAndFreeBuffers();

  isInsideFrame_ = false;

  return cmdBuffer->lastSubmitHandle_.handle();
}

void CommandQueue::enhancedShaderDebuggingPass(const igl::vulkan::VulkanContext& ctx,
                                               const igl::vulkan::CommandBuffer* cmdBuffer) {
  IGL_PROFILER_FUNCTION();

  auto& debugger = ctx.enhancedShaderDebuggingStore_;

  if (!cmdBuffer->getFramebuffer()) {
    return;
  }

  // If there are no color attachments, return, as we won't have a framebuffer to render into
  const auto indices = cmdBuffer->getFramebuffer()->getColorAttachmentIndices();
  if (indices.empty()) {
    return;
  }

  const auto min = std::min_element(indices.begin(), indices.end());

  const auto resolveAttachment = cmdBuffer->getFramebuffer()->getResolveColorAttachment(*min);
  std::shared_ptr<igl::IFramebuffer> framebuffer =
      resolveAttachment ? debugger->framebuffer(device_, resolveAttachment)
                        : cmdBuffer->getFramebuffer();

  igl::Result result;
  auto lineDrawingCmdBuffer =
      createCommandBuffer({"Command buffer: line drawing enhanced debugging"}, &result);

  if (!IGL_VERIFY(result.isOk())) {
    IGL_LOG_INFO("Error obtaining a new command buffer for drawing debug lines");
    return;
  }

  auto cmdEncoder = lineDrawingCmdBuffer->createRenderCommandEncoder(
      debugger->renderPassDesc(framebuffer), framebuffer);

  auto pipeline = debugger->pipeline(device_, framebuffer);
  cmdEncoder->bindRenderPipelineState(pipeline);

  {
    // Bind the line buffer
    auto vkEncoder = static_cast<RenderCommandEncoder*>(cmdEncoder.get());
    vkEncoder->binder().bindStorageBuffer(
        EnhancedShaderDebuggingStore::kBufferIndex,
        static_cast<igl::vulkan::Buffer*>(debugger->vertexBuffer().get()),
        sizeof(EnhancedShaderDebuggingStore::Header),
        0);

    cmdEncoder->pushDebugGroupLabel("Render Debug Lines", kColorDebugLines);
    cmdEncoder->bindDepthStencilState(debugger->depthStencilState());

    // Disable incrementing the draw call count
    const auto resetDrawCallCountValue = vkEncoder->setDrawCallCountEnabled(false);
    IGL_SCOPE_EXIT {
      vkEncoder->setDrawCallCountEnabled(resetDrawCallCountValue);
    };
    cmdEncoder->multiDrawIndirect(
        *debugger->vertexBuffer(), sizeof(EnhancedShaderDebuggingStore::Metadata), 1, 0);
  }
  cmdEncoder->popDebugGroupLabel();
  cmdEncoder->endEncoding();

  auto resetCmdBuffer = static_cast<CommandBuffer*>(lineDrawingCmdBuffer.get());
  const auto vkResetCmdBuffer = resetCmdBuffer->getVkCommandBuffer();

  // End the render pass by transitioning the surface that was presented by the application
  if (cmdBuffer->getPresentedSurface()) {
    resetCmdBuffer->present(cmdBuffer->getPresentedSurface());
  }

  // Barrier to ensure we have finished rendering the lines before we clear the buffer
  auto lineBuffer = static_cast<vulkan::Buffer*>(debugger->vertexBuffer().get());
  ivkBufferMemoryBarrier(&ctx.vf_,
                         vkResetCmdBuffer,
                         lineBuffer->getVkBuffer(),
                         0, /* src access flag */
                         0, /* dst access flag */
                         0,
                         VK_WHOLE_SIZE,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Reset instanceCount of the buffer
  ctx.vf_.vkCmdFillBuffer(vkResetCmdBuffer,
                          lineBuffer->getVkBuffer(),
                          offsetof(EnhancedShaderDebuggingStore::Header, command_) +
                              offsetof(VkDrawIndirectCommand, instanceCount),
                          sizeof(uint32_t), // reset only the instance count
                          0);

  endCommandBuffer(ctx, resetCmdBuffer, true);
}

} // namespace vulkan
} // namespace igl
