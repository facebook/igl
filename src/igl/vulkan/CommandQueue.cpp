/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/RenderCommandEncoder.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace igl {
namespace vulkan {

CommandQueue::CommandQueue(Device& device, CommandQueueType type) :
  device_(device), queueType_(type) {
  IGL_ASSERT(queueType_ == CommandQueueType::Graphics || queueType_ == CommandQueueType::Compute);
}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                  Result* outResult) {
  IGL_PROFILER_FUNCTION();

  // for now, we want only 1 command buffer
  IGL_ASSERT(!isInsideFrame_);

  isInsideFrame_ = true;

  return std::make_shared<CommandBuffer>(device_.getVulkanContext(), desc);
}

void CommandQueue::submit(const ICommandBuffer& cmdBuffer, bool /* endOfFrame */) {
  IGL_PROFILER_FUNCTION();
  VulkanContext& ctx = device_.getVulkanContext();

  IGL_ASSERT(isInsideFrame_);

  auto* vkCmdBuffer =
      const_cast<vulkan::CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&cmdBuffer));

  endCommandBuffer(ctx, vkCmdBuffer, true);
}

void CommandQueue::endCommandBuffer(const igl::vulkan::VulkanContext& ctx,
                                    igl::vulkan::CommandBuffer* cmdBuffer,
                                    bool present) {
  IGL_PROFILER_FUNCTION();

  const bool isGraphicsQueue = queueType_ == CommandQueueType::Graphics;

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
  ctx.DUBs_->markSubmit(cmdBuffer->lastSubmitHandle_);
  ctx.processDeferredTasks();

  isInsideFrame_ = false;
}

} // namespace vulkan
} // namespace igl
