/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSemaphore.h"

#include <igl/vulkan/Common.h>

namespace igl::vulkan {

VulkanSemaphore::VulkanSemaphore(const VulkanFunctionTable& vfIn,
                                 VkDevice deviceIn,
                                 bool exportableIn,
                                 const char* debugName) :
  vf(&vfIn), device(deviceIn), exportable(exportableIn) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkExportSemaphoreCreateInfo exportInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
  };
  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = exportableIn ? &exportInfo : nullptr,
      .flags = 0,
  };
  VK_ASSERT(vf->vkCreateSemaphore(device, &ci, nullptr, &vkSemaphore));
  VK_ASSERT(ivkSetDebugObjectName(
      vf, device, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)vkSemaphore, debugName));
}

VulkanSemaphore::VulkanSemaphore(const VulkanFunctionTable& vfIn,
                                 VkDevice deviceIn,
                                 uint64_t initialValue,
                                 bool exportableIn,
                                 const char* debugName) :
  vf(&vfIn), device(deviceIn), exportable(exportableIn) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkExportSemaphoreCreateInfo exportInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
  };
  const VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = exportableIn ? &exportInfo : nullptr,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = initialValue,
  };
  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &semaphoreTypeCreateInfo,
      .flags = 0,
  };
  VK_ASSERT(vf->vkCreateSemaphore(device, &ci, nullptr, &vkSemaphore));
  VK_ASSERT(ivkSetDebugObjectName(
      vf, device, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)vkSemaphore, debugName));
}

VulkanSemaphore ::~VulkanSemaphore() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (device != VK_NULL_HANDLE) {
    // lifetimes of all VkSemaphore objects are managed explicitly
    // we do not use deferredTask() for them
    vf->vkDestroySemaphore(device, vkSemaphore, nullptr);
  }
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept {
  std::swap(vf, other.vf);
  std::swap(device, other.device);
  std::swap(vkSemaphore, other.vkSemaphore);
  std::swap(exportable, other.exportable);
}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept {
  VulkanSemaphore tmp(std::move(other));
  std::swap(vf, tmp.vf);
  std::swap(device, tmp.device);
  std::swap(vkSemaphore, tmp.vkSemaphore);
  std::swap(exportable, other.exportable);
  return *this;
}

VkSemaphore VulkanSemaphore::getVkSemaphore() const noexcept {
  return vkSemaphore;
}

// Exportable semaphores are not used right now, so exclude from coverage
// FIXME_DEPRECATED_COVERAGE_EXCLUDE_START
int VulkanSemaphore::getFileDescriptor() const noexcept {
  if (!exportable) {
    return -1;
  }
  const VkSemaphoreGetFdInfoKHR fdInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
      .pNext = nullptr,
      .semaphore = vkSemaphore,
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
  };
  int fd = -1;
  const VkResult ok = vf->vkGetSemaphoreFdKHR(device, &fdInfo, &fd);
  if (ok == VK_SUCCESS) {
    return fd;
  }
  return -1;
}
// FIXME_DEPRECATED_COVERAGE_EXCLUDE_END

} // namespace igl::vulkan
