/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImmediateCommands.h"

#include <igl/vulkan/Common.h>
#include <utility>

namespace igl::vulkan {

VulkanImmediateCommands::VulkanImmediateCommands(const VulkanFunctionTable& vf,
                                                 VkDevice device,
                                                 uint32_t queueFamilyIndex,
                                                 bool exportableFences,
                                                 const char* debugName) :
  vf_(vf),
  device_(device),
  commandPool_(vf_,
               device_,
               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
               queueFamilyIndex,
               debugName),
  debugName_(debugName) {
  IGL_PROFILER_FUNCTION();

  vf_.vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue_);

  buffers_.reserve(kMaxCommandBuffers);

  for (uint32_t i = 0; i != kMaxCommandBuffers; i++) {
    buffers_.emplace_back(
        VulkanFence(vf_,
                    device_,
                    VkFenceCreateFlagBits{},
                    exportableFences,
                    IGL_FORMAT("Fence: commandBuffer #{}", i).c_str()),
        VulkanSemaphore(
            vf_, device, false, IGL_FORMAT("Semaphore: {} ({})", debugName, i).c_str()));
    VK_ASSERT(ivkAllocateCommandBuffer(
        &vf_, device_, commandPool_.getVkCommandPool(), &buffers_[i].cmdBufAllocated_));
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

    const VkResult result = vf_.vkWaitForFences(device_, 1, &buf.fence_.vkFence_, VK_TRUE, 0);

    if (result == VK_SUCCESS) {
      VK_ASSERT(vf_.vkResetCommandBuffer(buf.cmdBuf_, VkCommandBufferResetFlags{0}));
      VK_ASSERT(vf_.vkResetFences(device_, 1, &buf.fence_.vkFence_));
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
    IGL_LOG_INFO("Waiting for command buffers...\n");
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
  VK_ASSERT(ivkBeginCommandBuffer(&vf_, current->cmdBuf_));

  return *current;
}

void VulkanImmediateCommands::wait(const SubmitHandle handle, uint64_t timeoutNanoseconds) {
  if (isReady(handle)) {
    return;
  }

  if (!IGL_VERIFY(!buffers_[handle.bufferIndex_].isEncoding_)) {
    // we are waiting for a buffer which has not been submitted - this is probably a logic error
    // somewhere in the calling code
    return;
  }

  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

  VK_ASSERT(vf_.vkWaitForFences(
      device_, 1, &buffers_[handle.bufferIndex_].fence_.vkFence_, VK_TRUE, timeoutNanoseconds));

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
    VK_ASSERT(vf_.vkWaitForFences(device_, numFences, fences, VK_TRUE, UINT64_MAX));
  }

  purge();
}

bool VulkanImmediateCommands::isRecycled(SubmitHandle handle) const {
  IGL_ASSERT(handle.bufferIndex_ < kMaxCommandBuffers);

  if (handle.empty()) {
    // a null handle
    return true;
  }

  // already recycled and reused by another command buffer
  return buffers_[handle.bufferIndex_].handle_.submitId_ != handle.submitId_;
}

bool VulkanImmediateCommands::isReady(const SubmitHandle handle) const {
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

  return vf_.vkWaitForFences(device_, 1, &buf.fence_.vkFence_, VK_TRUE, 0) == VK_SUCCESS;
}

VulkanImmediateCommands::SubmitHandle VulkanImmediateCommands::submit(
    const CommandBufferWrapper& wrapper) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_SUBMIT);
  IGL_ASSERT(wrapper.isEncoding_);
  VK_ASSERT(ivkEndCommandBuffer(&vf_, wrapper.cmdBuf_));

  // @lint-ignore CLANGTIDY
  const VkPipelineStageFlags waitStageMasks[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
  // @lint-ignore CLANGTIDY
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
  // @lint-ignore CLANGTIDY
  const VkFence vkFence = wrapper.fence_.vkFence_;
  IGL_PROFILER_ZONE("vkQueueSubmit()", IGL_PROFILER_COLOR_SUBMIT);
#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkQueueSubmit()\n\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
  VK_ASSERT(vf_.vkQueueSubmit(queue_, 1u, &si, vkFence));
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

VkFence VulkanImmediateCommands::getVkFenceFromSubmitHandle(SubmitHandle handle) {
  IGL_ASSERT(handle.bufferIndex_ < buffers_.size());

  if (isRecycled(handle)) {
    return VK_NULL_HANDLE;
  }

  return buffers_[handle.bufferIndex_].fence_.vkFence_;
}

} // namespace igl::vulkan
