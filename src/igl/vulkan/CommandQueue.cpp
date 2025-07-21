/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/CommandQueue.h>

#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>

#define IGL_COMMAND_QUEUE_DEBUG_FENCES (IGL_DEBUG && 0)

#if IGL_COMMAND_QUEUE_DEBUG_FENCES
#include <cinttypes> // PRIx3
#include <igl/vulkan/VulkanFence.h>
#endif // IGL_COMMAND_QUEUE_DEBUG_FENCES

namespace igl::vulkan {

CommandQueue::CommandQueue(Device& device, const CommandQueueDesc& /*desc*/) : device_(device) {}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                  Result* /*outResult*/) {
  IGL_PROFILER_FUNCTION();

  ++numBuffersLeftToSubmit_;

  return std::make_shared<CommandBuffer>(device_.getVulkanContext(), desc);
}

SubmitHandle CommandQueue::submit(const ICommandBuffer& cmdBuffer, bool /* endOfFrame */) {
  IGL_PROFILER_FUNCTION();
  VulkanContext& ctx = device_.getVulkanContext();

  if (ctx.enhancedShaderDebuggingStore_) {
    ctx.enhancedShaderDebuggingStore_->installBufferBarrier(cmdBuffer);
  }

  incrementDrawCount(cmdBuffer.getCurrentDrawCount());

  --numBuffersLeftToSubmit_;

  auto* vkCmdBuffer =
      const_cast<CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&cmdBuffer));

#if IGL_COMMAND_QUEUE_DEBUG_FENCES
  // Create label with Fence handle and Fence FD, if available
  // A string such as "Submit command buffer (hex: 0x149b90a10, fd: 12345)" has 55 characters
  char labelName[60];
  if (vkCmdBuffer->wrapper_.fence_.exportable()) {
    std::snprintf(labelName,
                  sizeof(labelName),
                  "Submit command buffer (hex: %#" PRIx64 ", fd: %d)",
                  reinterpret_cast<uint64_t>(vkCmdBuffer->wrapper_.fence_.vkFence_),
                  ctx.immediate_->cachedFDFromSubmitHandle(vkCmdBuffer->wrapper_.handle_));
  } else {
    std::snprintf(labelName,
                  sizeof(labelName),
                  "Submit command buffer (hex: %#" PRIx64 ")",
                  reinterpret_cast<uint64_t>(vkCmdBuffer->wrapper_.fence_.vkFence_));
  }

  ivkCmdInsertDebugUtilsLabel(&ctx.vf_,
                              vkCmdBuffer->getVkCommandBuffer(),
                              labelName,
                              kColorCommandBufferSubmissionWithFence.toFloatPtr());
#endif // IGL_COMMAND_QUEUE_DEBUG_FENCES

  const bool presentIfNotDebugging = ctx.enhancedShaderDebuggingStore_ == nullptr;
  auto submitHandle = endCommandBuffer(ctx, vkCmdBuffer, presentIfNotDebugging);

  if (ctx.enhancedShaderDebuggingStore_) {
    ctx.enhancedShaderDebuggingStore_->enhancedShaderDebuggingPass(*this, vkCmdBuffer);
  }

  return submitHandle;
}

SubmitHandle CommandQueue::endCommandBuffer(VulkanContext& ctx,
                                            CommandBuffer* cmdBuffer,
                                            bool present) {
  IGL_PROFILER_FUNCTION();

  // Submit to the graphics queue.
  const bool shouldPresent = ctx.hasSwapchain() && cmdBuffer->isFromSwapchain() && present;
  if (shouldPresent) {
    if (ctx.timelineSemaphore_) {
      // if we are presenting a swapchain image, signal our timeline semaphore
      const uint64_t signalValue =
          ctx.swapchain_->getFrameNumber() + ctx.swapchain_->getNumSwapchainImages();
      // we wait for this value next time we want to acquire this swapchain image
      ctx.swapchain_->timelineWaitValues_[ctx.swapchain_->getCurrentImageIndex()] = signalValue;
      ctx.immediate_->signalSemaphore(ctx.timelineSemaphore_->getVkSemaphore(), signalValue);
    } else {
      // this can be removed once we switch to timeline semaphores
      ctx.immediate_->waitSemaphore(ctx.swapchain_->getSemaphore());
    }
  }

  cmdBuffer->lastSubmitHandle_ = ctx.immediate_->submit(cmdBuffer->wrapper_);

  if (shouldPresent) {
    ctx.present();
  }
  ctx.syncMarkSubmitted(cmdBuffer->lastSubmitHandle_);
  ctx.processDeferredTasks();
  ctx.stagingDevice_->mergeRegionsAndFreeBuffers();

  return cmdBuffer->lastSubmitHandle_.handle();
}

} // namespace igl::vulkan
