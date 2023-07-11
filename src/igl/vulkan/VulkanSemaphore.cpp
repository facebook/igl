/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSemaphore.h"

#include <igl/vulkan/Common.h>

namespace igl::vulkan {

VulkanSemaphore::VulkanSemaphore(VkDevice device, const char* debugName) : device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .flags = 0,
  };
  VK_ASSERT(vkCreateSemaphore(device, &ci, nullptr, &vkSemaphore_));
  VK_ASSERT(
      ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)vkSemaphore_, debugName));
}

VulkanSemaphore ::~VulkanSemaphore() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (device_ != VK_NULL_HANDLE) {
    // lifetimes of all VkSemaphore objects are managed explicitly
    // we do not use deferredTask() for them
    vkDestroySemaphore(device_, vkSemaphore_, nullptr);
  }
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept {
  std::swap(device_, other.device_);
  std::swap(vkSemaphore_, other.vkSemaphore_);
}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept {
  VulkanSemaphore tmp(std::move(other));
  std::swap(device_, tmp.device_);
  std::swap(vkSemaphore_, tmp.vkSemaphore_);
  return *this;
}

} // namespace igl::vulkan
