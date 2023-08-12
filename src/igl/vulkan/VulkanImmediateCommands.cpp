/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImmediateCommands.h"

#include <igl/vulkan/Common.h>
#include <lvk/vulkan/VulkanUtils.h>

#include <utility>

namespace lvk {
namespace vulkan {

VulkanImmediateCommands::VulkanImmediateCommands(VkDevice device, uint32_t queueFamilyIndex, const char* debugName) :
  device_(device), queueFamilyIndex_(queueFamilyIndex), debugName_(debugName) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue_);

  const VkCommandPoolCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = queueFamilyIndex,
  };
  VK_ASSERT(vkCreateCommandPool(device, &ci, nullptr, &commandPool_));
  ivkSetDebugObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)commandPool_, debugName);

  const VkCommandBufferAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  for (uint32_t i = 0; i != kMaxCommandBuffers; i++) {
    auto& buf = buffers_[i];
    char fenceName[256] = {0};
    char semaphoreName[256] = {0};
    if (debugName) {
      snprintf(fenceName, sizeof(fenceName) - 1, "Fence: %s (cmdbuf %u)", debugName, i);
      snprintf(semaphoreName, sizeof(semaphoreName) - 1, "Semaphore: %s (cmdbuf %u)", debugName, i);
    }
    buf.semaphore_ = lvk::createSemaphore(device, semaphoreName);
    buf.fence_ = lvk::createFence(device, fenceName);
    VK_ASSERT(vkAllocateCommandBuffers(device, &ai, &buf.cmdBufAllocated_));
    buffers_[i].handle_.bufferIndex_ = i;
  }
}

VulkanImmediateCommands::~VulkanImmediateCommands() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  waitAll();

  for (auto& buf : buffers_) {
    // lifetimes of all VkFence objects are managed explicitly
    // we do not use deferredTask() for them
    vkDestroyFence(device_, buf.fence_, nullptr);
    vkDestroySemaphore(device_, buf.semaphore_, nullptr);
  }

  vkDestroyCommandPool(device_, commandPool_, nullptr);
}

void VulkanImmediateCommands::purge() {
  LVK_PROFILER_FUNCTION();

  for (auto& buf : buffers_) {
    if (buf.cmdBuf_ == VK_NULL_HANDLE || buf.isEncoding_) {
      continue;
    }

    const VkResult result = vkWaitForFences(device_, 1, &buf.fence_, VK_TRUE, 0);

    if (result == VK_SUCCESS) {
      VK_ASSERT(vkResetCommandBuffer(buf.cmdBuf_, VkCommandBufferResetFlags{0}));
      VK_ASSERT(vkResetFences(device_, 1, &buf.fence_));
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
  LVK_PROFILER_FUNCTION();

  if (!numAvailableCommandBuffers_) {
    purge();
  }

  while (!numAvailableCommandBuffers_) {
    LLOGL("Waiting for command buffers...\n");
    LVK_PROFILER_ZONE("Waiting for command buffers...", LVK_PROFILER_COLOR_WAIT);
    purge();
    LVK_PROFILER_ZONE_END();
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
  const VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_ASSERT(vkBeginCommandBuffer(current->cmdBuf_, &bi));

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

  VK_ASSERT(vkWaitForFences(device_, 1, &buffers_[handle.bufferIndex_].fence_, VK_TRUE, UINT64_MAX));

  purge();
}

void VulkanImmediateCommands::waitAll() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_WAIT);

  // @lint-ignore CLANGTIDY
  VkFence fences[kMaxCommandBuffers];

  uint32_t numFences = 0;

  for (const auto& buf : buffers_) {
    if (buf.cmdBuf_ != VK_NULL_HANDLE && !buf.isEncoding_) {
      fences[numFences++] = buf.fence_;
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

  return vkWaitForFences(device_, 1, &buf.fence_, VK_TRUE, 0) == VK_SUCCESS;
}

VulkanImmediateCommands::SubmitHandle VulkanImmediateCommands::submit(const CommandBufferWrapper& wrapper) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_SUBMIT);
  IGL_ASSERT(wrapper.isEncoding_);
  VK_ASSERT(vkEndCommandBuffer(wrapper.cmdBuf_));

  const VkPipelineStageFlags waitStageMasks[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
  VkSemaphore waitSemaphores[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  uint32_t numWaitSemaphores = 0;
  if (waitSemaphore_) {
    waitSemaphores[numWaitSemaphores++] = waitSemaphore_;
  }
  if (lastSubmitSemaphore_) {
    waitSemaphores[numWaitSemaphores++] = lastSubmitSemaphore_;
  }

  LVK_PROFILER_ZONE("vkQueueSubmit()", LVK_PROFILER_COLOR_SUBMIT);
#if IGL_VULKAN_PRINT_COMMANDS
  LLOGL("%p vkQueueSubmit()\n\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
  const VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = numWaitSemaphores,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStageMasks,
      .commandBufferCount = 1u,
      .pCommandBuffers = &wrapper.cmdBuf_,
      .signalSemaphoreCount = 1u,
      .pSignalSemaphores = &wrapper.semaphore_,
  };
  VK_ASSERT(vkQueueSubmit(queue_, 1u, &si, wrapper.fence_));
  LVK_PROFILER_ZONE_END();

  lastSubmitSemaphore_ = wrapper.semaphore_;
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
} // namespace lvk
