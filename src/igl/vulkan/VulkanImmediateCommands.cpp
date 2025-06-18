/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImmediateCommands.h"

#include <utility>
#include <igl/vulkan/Common.h>

namespace igl::vulkan {

VulkanImmediateCommands::VulkanImmediateCommands(const VulkanFunctionTable& vf,
                                                 VkDevice device,
                                                 uint32_t queueFamilyIndex,
                                                 bool exportableFences,
                                                 bool useTimelineSemaphoreAndSynchronization2,
                                                 const char* debugName) :
  vf_(vf),
  device_(device),
  commandPool_(vf_,
               device_,
               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
               queueFamilyIndex,
               debugName),
  debugName_(debugName),
  lastSubmitSemaphore_({
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = VK_NULL_HANDLE,
      .value = 0ull,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      .deviceIndex = 0ul,
  }),
  waitSemaphore_({
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = VK_NULL_HANDLE,
      .value = 0ull,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      .deviceIndex = 0ul,
  }),
  signalSemaphore_({
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = VK_NULL_HANDLE,
      .value = 0ull,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      .deviceIndex = 0ul,
  }),
  useTimelineSemaphoreAndSynchronization2_(useTimelineSemaphoreAndSynchronization2) {
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
  // IGL_DEBUG_ASSERT(nextSubmitHandle_.empty(),
  //                "VulkanImmediateCommands::acquire() is not reentrant. You should submit() the "
  //                "previous buffer before calling acquire() again.");

  // if (IGL_DEBUG_VERIFY_NOT(!nextSubmitHandle_.empty())) {
  if (!nextSubmitHandle_.empty()) {
    IGL_LOG_ERROR(
        "VulkanImmediateCommands::acquire() is not reentrant. You should submit() the "
        "previous buffer before calling acquire() again.");
  }

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

  IGL_DEBUG_ASSERT(numAvailableCommandBuffers_, "No available command buffers");
  IGL_DEBUG_ASSERT(current, "No available command buffers");
  IGL_DEBUG_ASSERT(current->cmdBufAllocated_ != VK_NULL_HANDLE);

  current->handle_.submitId_ = submitCounter_;
  numAvailableCommandBuffers_--;

  current->cmdBuf_ = current->cmdBufAllocated_;
  current->isEncoding_ = true;
  current->fd = -1;

  const VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_ASSERT(vf_.vkBeginCommandBuffer(current->cmdBuf_, &bi));

  nextSubmitHandle_ = current->handle_;
  return *current;
}

VkResult VulkanImmediateCommands::wait(const SubmitHandle handle, uint64_t timeoutNanoseconds) {
  if (isReady(handle)) {
    return VK_SUCCESS;
  }

  if (!IGL_DEBUG_VERIFY(!buffers_[handle.bufferIndex_].isEncoding_)) {
    // we are waiting for a buffer which has not been submitted - this is probably a logic error
    // somewhere in the calling code
    return VK_ERROR_UNKNOWN;
  }

  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

  const VkResult fenceResult = vf_.vkWaitForFences(
      device_, 1, &buffers_[handle.bufferIndex_].fence_.vkFence_, VK_TRUE, timeoutNanoseconds);

  if (fenceResult == VK_TIMEOUT) {
    return VK_TIMEOUT;
  }

  if (fenceResult != VK_SUCCESS) {
    IGL_LOG_ERROR_ONCE(
        "VulkanImmediateCommands::wait - Waiting for command buffer fence failed with error %i",
        int(fenceResult));
    // Intentional fallthrough: we must purge so that we can release command buffers.
  }

  purge();

  return fenceResult;
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
  IGL_DEBUG_ASSERT(handle.bufferIndex_ < kMaxCommandBuffers);

  if (handle.empty()) {
    // a null handle
    return true;
  }

  // already recycled and reused by another command buffer
  return buffers_[handle.bufferIndex_].handle_.submitId_ != handle.submitId_;
}

bool VulkanImmediateCommands::isReady(const SubmitHandle handle) const {
  IGL_DEBUG_ASSERT(handle.bufferIndex_ < kMaxCommandBuffers);

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

  IGL_DEBUG_ASSERT(wrapper.isEncoding_);
  VK_ASSERT(vf_.vkEndCommandBuffer(wrapper.cmdBuf_));

  if (useTimelineSemaphoreAndSynchronization2_) {
    // @lint-ignore CLANGTIDY
    VkSemaphoreSubmitInfo waitSemaphores[] = {{}, {}};
    uint32_t numWaitSemaphores = 0;
    if (waitSemaphore_.semaphore) {
      waitSemaphores[numWaitSemaphores++] = waitSemaphore_;
    }
    if (lastSubmitSemaphore_.semaphore) {
      waitSemaphores[numWaitSemaphores++] = lastSubmitSemaphore_;
    }
    // @lint-ignore CLANGTIDY
    const VkSemaphoreSubmitInfo signalSemaphores[] = {
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = wrapper.semaphore_.getVkSemaphore(),
            .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        },
        signalSemaphore_,
    };

    const VkCommandBufferSubmitInfo bufferSI = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = wrapper.cmdBuf_,
    };
    const VkSubmitInfo2 si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = numWaitSemaphores,
        .pWaitSemaphoreInfos = waitSemaphores,
        .commandBufferInfoCount = 1u,
        .pCommandBufferInfos = &bufferSI,
        .signalSemaphoreInfoCount = signalSemaphore_.semaphore ? 2u : 1u,
        .pSignalSemaphoreInfos = signalSemaphores,
    };

    IGL_PROFILER_ZONE("vkQueueSubmit2KHR()", IGL_PROFILER_COLOR_SUBMIT);
#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkQueueSubmit2KHR()\n\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
    VK_ASSERT(vf_.vkQueueSubmit2KHR(queue_, 1u, &si, wrapper.fence_.vkFence_));
    IGL_PROFILER_ZONE_END();
  } else {
    // @lint-ignore CLANGTIDY
    const VkPipelineStageFlags waitStageMasks[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
    // @lint-ignore CLANGTIDY
    VkSemaphore waitSemaphores[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    uint32_t numWaitSemaphores = 0;
    if (waitSemaphore_.semaphore) {
      waitSemaphores[numWaitSemaphores++] = waitSemaphore_.semaphore;
    }
    if (lastSubmitSemaphore_.semaphore) {
      waitSemaphores[numWaitSemaphores++] = lastSubmitSemaphore_.semaphore;
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
  }

  lastSubmitSemaphore_.semaphore = wrapper.semaphore_.vkSemaphore_;
  lastSubmitHandle_ = wrapper.handle_;
  waitSemaphore_.semaphore = VK_NULL_HANDLE;
  signalSemaphore_.semaphore = VK_NULL_HANDLE;

  // reset
  const_cast<CommandBufferWrapper&>(wrapper).isEncoding_ = false;
  submitCounter_++;

  if (!submitCounter_) {
    // skip the 0 value - when uint32_t wraps around (null SubmitHandle)
    submitCounter_++;
  }

  nextSubmitHandle_ = {};

  return lastSubmitHandle_;
}

void VulkanImmediateCommands::waitSemaphore(VkSemaphore semaphore) {
  IGL_DEBUG_ASSERT(waitSemaphore_.semaphore == VK_NULL_HANDLE);

  waitSemaphore_.semaphore = semaphore;
}

void VulkanImmediateCommands::signalSemaphore(VkSemaphore semaphore, uint64_t signalValue) {
  IGL_DEBUG_ASSERT(signalSemaphore_.semaphore == VK_NULL_HANDLE);
  signalSemaphore_.semaphore = semaphore;
  signalSemaphore_.value = signalValue;
}

VkSemaphore VulkanImmediateCommands::acquireLastSubmitSemaphore() {
  return std::exchange(lastSubmitSemaphore_.semaphore, VK_NULL_HANDLE);
}

VulkanImmediateCommands::SubmitHandle VulkanImmediateCommands::getLastSubmitHandle() const {
  return lastSubmitHandle_;
}

VulkanImmediateCommands::SubmitHandle VulkanImmediateCommands::getNextSubmitHandle() const {
  return nextSubmitHandle_.empty() ? lastSubmitHandle_ : nextSubmitHandle_;
}

VkFence VulkanImmediateCommands::getVkFenceFromSubmitHandle(SubmitHandle handle) {
  IGL_DEBUG_ASSERT(handle.bufferIndex_ < buffers_.size());

  if (isRecycled(handle)) {
    return VK_NULL_HANDLE;
  }

  return buffers_[handle.bufferIndex_].fence_.vkFence_;
}

void VulkanImmediateCommands::storeFDInSubmitHandle(SubmitHandle handle, int fd) noexcept {
  IGL_DEBUG_ASSERT(handle.bufferIndex_ < buffers_.size());
  buffers_[handle.bufferIndex_].fd = fd;
}

int VulkanImmediateCommands::cachedFDFromSubmitHandle(SubmitHandle handle) const noexcept {
  IGL_DEBUG_ASSERT(handle.bufferIndex_ < buffers_.size());
  return buffers_[handle.bufferIndex_].fd;
}

} // namespace igl::vulkan
