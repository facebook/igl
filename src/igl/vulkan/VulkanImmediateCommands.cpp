/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImmediateCommands.h"

#include <igl/vulkan/Common.h>

#include <format>
#include <utility>

namespace igl {
namespace vulkan {

VulkanImmediateCommands::VulkanImmediateCommands(VkDevice device,
                                                 uint32_t queueFamilyIndex,
                                                 const char* debugName) :
  device_(device),
  commandPool_(device_,
               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
               queueFamilyIndex,
               debugName),
  debugName_(debugName) {
  IGL_PROFILER_FUNCTION();

  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue_);

  buffers_.reserve(kMaxCommandBuffers);

  for (uint32_t i = 0; i != kMaxCommandBuffers; i++) {
    buffers_.emplace_back(
        VulkanFence(
            device_, VkFenceCreateFlagBits{}, std::format("Fence: commandBuffer #{}", i).c_str()),
        VulkanSemaphore(device, std::format("Semaphore: {} ({})", debugName, i).c_str()));
    VK_ASSERT(ivkAllocateCommandBuffer(
        device_, commandPool_.getVkCommandPool(), &buffers_[i].cmdBufAllocated_));
    buffers_[i].handle_.bufferIndex_ = i;
  }
}

VulkanImmediateCommands::~VulkanImmediateCommands() {
  waitAll();
}

void VulkanImmediateCommands::purge() {
  IGL_PROFILER_FUNCTION();

  for (auto& buf : buffers_) {
    if (buf.cmdBuf_ == VK_NULL_HANDLE || buf.isEncoding_) {
      continue;
    }

    const VkResult result = vkWaitForFences(device_, 1, &buf.fence_.vkFence_, VK_TRUE, 0);

    if (result == VK_SUCCESS) {
      VK_ASSERT(vkResetCommandBuffer(buf.cmdBuf_, VkCommandBufferResetFlags{0}));
      VK_ASSERT(vkResetFences(device_, 1, &buf.fence_.vkFence_));
      buf.cmdBuf_ = VK_NULL_HANDLE;
      numAvailableCommandBuffers_++;
    } else {
      if (result != VK_TIMEOUT) {
        VK_ASSERT(result);
      }
    }
  }
}

const VulkanImmediateCommands::CommandBufferWrapper& VulkanImmediateCommands::acquire() {
  IGL_PROFILER_FUNCTION();

  if (!numAvailableCommandBuffers_) {
    purge();
  }

  while (!numAvailableCommandBuffers_) {
    LLOGL("Waiting for command buffers...\n");
    IGL_PROFILER_ZONE("Waiting for command buffers...", IGL_PROFILER_COLOR_WAIT);
    purge();
    IGL_PROFILER_ZONE_END();
  }

  VulkanImmediateCommands::CommandBufferWrapper* current = nullptr;

  // we are ok with any available buffer
  for (auto& buf : buffers_) {
    if (buf.cmdBuf_ == VK_NULL_HANDLE) {
      current = &buf;
      break;
    }
  }

  // make clang happy
  assert(current);

  IGL_ASSERT_MSG(numAvailableCommandBuffers_, "No available command buffers");
  IGL_ASSERT_MSG(current, "No available command buffers");
  IGL_ASSERT(current->cmdBufAllocated_ != VK_NULL_HANDLE);

  current->handle_.submitId_ = submitCounter_;
  numAvailableCommandBuffers_--;

  current->cmdBuf_ = current->cmdBufAllocated_;
  current->isEncoding_ = true;
  VK_ASSERT(ivkBeginCommandBuffer(current->cmdBuf_));

  return *current;
}

void VulkanImmediateCommands::wait(const SubmitHandle handle) {
  if (isReady(handle)) {
    return;
  }

  if (!IGL_VERIFY(!buffers_[handle.bufferIndex_].isEncoding_)) {
    // we are waiting for a buffer which has not been submitted - this is probably a logic error
    // somewhere in the calling code
    return;
  }

  VK_ASSERT(vkWaitForFences(
      device_, 1, &buffers_[handle.bufferIndex_].fence_.vkFence_, VK_TRUE, UINT64_MAX));

  purge();
}

void VulkanImmediateCommands::waitAll() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

  // @lint-ignore CLANGTIDY
  VkFence fences[kMaxCommandBuffers];

  uint32_t numFences = 0;

  for (const auto& buf : buffers_) {
    if (buf.cmdBuf_ != VK_NULL_HANDLE && !buf.isEncoding_) {
      fences[numFences++] = buf.fence_.vkFence_;
    }
  }

  if (numFences) {
    VK_ASSERT(vkWaitForFences(device_, numFences, fences, VK_TRUE, UINT64_MAX));
  }

  purge();
}

bool VulkanImmediateCommands::isReady(const SubmitHandle handle, bool fastCheckNoVulkan) const {
  IGL_ASSERT(handle.bufferIndex_ < kMaxCommandBuffers);

  if (handle.empty()) {
    // a null handle
    return true;
  }

  const CommandBufferWrapper& buf = buffers_[handle.bufferIndex_];

  if (buf.cmdBuf_ == VK_NULL_HANDLE) {
    // already recycled and not yet reused
    return true;
  }

  if (buf.handle_.submitId_ != handle.submitId_) {
    // already recycled and reused by another command buffer
    return true;
  }

  if (fastCheckNoVulkan) {
    // do not ask the Vulkan API about it, just let it retire naturally (when submitId for this
    // bufferIndex gets incremented)
    return false;
  }

  return vkWaitForFences(device_, 1, &buf.fence_.vkFence_, VK_TRUE, 0) == VK_SUCCESS;
}

VulkanImmediateCommands::SubmitHandle VulkanImmediateCommands::submit(
    const CommandBufferWrapper& wrapper) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_SUBMIT);
  IGL_ASSERT(wrapper.isEncoding_);
  VK_ASSERT(ivkEndCommandBuffer(wrapper.cmdBuf_));

  const VkPipelineStageFlags waitStageMasks[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
  VkSemaphore waitSemaphores[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  uint32_t numWaitSemaphores = 0;
  if (waitSemaphore_) {
    waitSemaphores[numWaitSemaphores++] = waitSemaphore_;
  }
  if (lastSubmitSemaphore_) {
    waitSemaphores[numWaitSemaphores++] = lastSubmitSemaphore_;
  }

  const VkSubmitInfo si = ivkGetSubmitInfo(&wrapper.cmdBuf_,
                                           numWaitSemaphores,
                                           waitSemaphores,
                                           waitStageMasks,
                                           &wrapper.semaphore_.vkSemaphore_);
  IGL_PROFILER_ZONE("vkQueueSubmit()", IGL_PROFILER_COLOR_SUBMIT);
#if IGL_VULKAN_PRINT_COMMANDS
  LLOGL("%p vkQueueSubmit()\n\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
  VK_ASSERT(vkQueueSubmit(queue_, 1u, &si, wrapper.fence_.vkFence_));
  IGL_PROFILER_ZONE_END();

  lastSubmitSemaphore_ = wrapper.semaphore_.vkSemaphore_;
  lastSubmitHandle_ = wrapper.handle_;
  waitSemaphore_ = VK_NULL_HANDLE;

  // reset
  const_cast<CommandBufferWrapper&>(wrapper).isEncoding_ = false;
  submitCounter_++;

  if (!submitCounter_) {
    // skip the 0 value - when uint32_t wraps around (null SubmitHandle)
    submitCounter_++;
  }

  return lastSubmitHandle_;
}

void VulkanImmediateCommands::waitSemaphore(VkSemaphore semaphore) {
  IGL_ASSERT(waitSemaphore_ == VK_NULL_HANDLE);

  waitSemaphore_ = semaphore;
}

VkSemaphore VulkanImmediateCommands::acquireLastSubmitSemaphore() {
  return std::exchange(lastSubmitSemaphore_, VK_NULL_HANDLE);
}

VulkanImmediateCommands::SubmitHandle VulkanImmediateCommands::getLastSubmitHandle() const {
  return lastSubmitHandle_;
}

} // namespace vulkan
} // namespace igl
