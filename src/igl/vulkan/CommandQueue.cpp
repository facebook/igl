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
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace igl::vulkan {

CommandQueue::CommandQueue(Device& device, const CommandQueueDesc& /*desc*/) : device_(device) {}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                  Result* /*outResult*/) {
  IGL_PROFILER_FUNCTION();

  // for now, we want only 1 command buffer
  IGL_DEBUG_ASSERT(!isInsideFrame_);

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

  IGL_DEBUG_ASSERT(isInsideFrame_);

  auto* vkCmdBuffer =
      const_cast<vulkan::CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&cmdBuffer));
  const bool presentIfNotDebugging = ctx.enhancedShaderDebuggingStore_ == nullptr;
  auto submitHandle = endCommandBuffer(ctx, vkCmdBuffer, presentIfNotDebugging);

  if (ctx.enhancedShaderDebuggingStore_) {
    ctx.enhancedShaderDebuggingStore_->enhancedShaderDebuggingPass(*this, vkCmdBuffer);
  }

  return submitHandle;
}

SubmitHandle CommandQueue::endCommandBuffer(igl::vulkan::VulkanContext& ctx,
                                            igl::vulkan::CommandBuffer* cmdBuffer,
                                            bool present) {
  IGL_PROFILER_FUNCTION();

  // Submit to the graphics queue.
  const bool shouldPresent = ctx.hasSwapchain() && cmdBuffer->isFromSwapchain() && present;
  if (shouldPresent) {
    ctx.immediate_->waitSemaphore(ctx.swapchain_->getSemaphore());
  }

  cmdBuffer->lastSubmitHandle_ = ctx.immediate_->submit(cmdBuffer->wrapper_);

  if (shouldPresent) {
    ctx.present();
  }
  ctx.syncMarkSubmitted(cmdBuffer->lastSubmitHandle_);
  ctx.processDeferredTasks();
  ctx.stagingDevice_->mergeRegionsAndFreeBuffers();

  isInsideFrame_ = false;

  return cmdBuffer->lastSubmitHandle_.handle();
}

} // namespace igl::vulkan
