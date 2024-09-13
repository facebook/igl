/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanFence.h"

#include <igl/vulkan/Common.h>
#include <utility> // std::swap

namespace igl::vulkan {

VulkanFence::VulkanFence(const VulkanFunctionTable& vf,
                         VkDevice device,
                         VkFlags flags,
                         bool exportable,
                         const char* debugName) :
  vf_(&vf), device_(device), exportable_(exportable) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkExportFenceCreateInfo exportInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
  };

  const VkFenceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = exportable ? &exportInfo : nullptr,
      .flags = flags,
  };
  VK_ASSERT(vf_->vkCreateFence(device, &ci, nullptr, &vkFence_));

  VK_ASSERT(
      ivkSetDebugObjectName(vf_, device_, VK_OBJECT_TYPE_FENCE, (uint64_t)vkFence_, debugName));
}

VulkanFence ::~VulkanFence() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (device_ != VK_NULL_HANDLE) {
    // lifetimes of all VkFence objects are managed explicitly
    // we do not use deferredTask() for them
    vf_->vkDestroyFence(device_, vkFence_, nullptr);
  }
}

VulkanFence::VulkanFence(VulkanFence&& other) noexcept {
  std::swap(vf_, other.vf_);
  std::swap(device_, other.device_);
  std::swap(vkFence_, other.vkFence_);
  std::swap(exportable_, other.exportable_);
}

VulkanFence& VulkanFence::operator=(VulkanFence&& other) noexcept {
  VulkanFence tmp(std::move(other));
  std::swap(vf_, tmp.vf_);
  std::swap(device_, tmp.device_);
  std::swap(vkFence_, tmp.vkFence_);
  std::swap(exportable_, tmp.exportable_);
  return *this;
}

bool VulkanFence::reset() noexcept {
  const VkResult result = vf_->vkResetFences(device_, 1, &vkFence_);
  return result == VK_SUCCESS;
}

bool VulkanFence::wait(uint64_t timeoutNs) noexcept {
  const VkResult result = vf_->vkWaitForFences(device_, 1, &vkFence_, VK_TRUE, timeoutNs);
  return result == VK_SUCCESS;
}

bool VulkanFence::signal(VkQueue queue) {
  if (queue == VK_NULL_HANDLE) {
    // protected against invalid submit
    return false;
  }

  const VkResult result = vf_->vkQueueSubmit(queue, 0, nullptr, vkFence_);
  return result == VK_SUCCESS;
}

} // namespace igl::vulkan
