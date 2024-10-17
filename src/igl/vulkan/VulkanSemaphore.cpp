/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSemaphore.h"

#include <igl/vulkan/Common.h>

namespace igl::vulkan {

VulkanSemaphore::VulkanSemaphore(const VulkanFunctionTable& vf,
                                 VkDevice device,
                                 bool exportable,
                                 const char* debugName) :
  vf_(&vf), device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkExportSemaphoreCreateInfo exportInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
  };
  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = exportable ? &exportInfo : nullptr,
      .flags = 0,
  };
  VK_ASSERT(vf_->vkCreateSemaphore(device, &ci, nullptr, &vkSemaphore_));
  VK_ASSERT(ivkSetDebugObjectName(
      vf_, device_, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)vkSemaphore_, debugName));
}

VulkanSemaphore ::~VulkanSemaphore() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (device_ != VK_NULL_HANDLE) {
    // lifetimes of all VkSemaphore objects are managed explicitly
    // we do not use deferredTask() for them
    vf_->vkDestroySemaphore(device_, vkSemaphore_, nullptr);
  }
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept {
  std::swap(vf_, other.vf_);
  std::swap(device_, other.device_);
  std::swap(vkSemaphore_, other.vkSemaphore_);
}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept {
  VulkanSemaphore tmp(std::move(other));
  std::swap(vf_, tmp.vf_);
  std::swap(device_, tmp.device_);
  std::swap(vkSemaphore_, tmp.vkSemaphore_);
  return *this;
}

VkSemaphore VulkanSemaphore::getVkSemaphore() const noexcept {
  return vkSemaphore_;
}

int VulkanSemaphore::getFileDescriptor() const noexcept {
  // This is intentionally c++17 compatible and not c++20 style
  // because there are libraries that rely on this code
  // that are not yet moved forward to c++20
  VkSemaphoreGetFdInfoKHR fdInfo;
  fdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
  fdInfo.pNext = nullptr;
  fdInfo.semaphore = vkSemaphore_;
  fdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  int fd = -1;
  auto ok = vf_->vkGetSemaphoreFdKHR(device_, &fdInfo, &fd);
  if (ok == VK_SUCCESS) {
    return fd;
  }
  return -1;
}

} // namespace igl::vulkan
