/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanCommandPool.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

VulkanCommandPool::VulkanCommandPool(const VulkanFunctionTable& vf,
                                     VkDevice device,
                                     VkCommandPoolCreateFlags flags,
                                     uint32_t queueFamilyIndex,
                                     const char* debugName) :
  vf_(vf), device_(device), queueFamilyIndex_(queueFamilyIndex) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateCommandPool(&vf_, device_, flags, queueFamilyIndex_, &commandPool_));

  ivkSetDebugObjectName(&vf_,
                        device,
                        VK_OBJECT_TYPE_COMMAND_POOL,
                        (uint64_t)commandPool_,
                        IGL_FORMAT("Command Pool: {}", debugName).c_str());
}

VulkanCommandPool ::~VulkanCommandPool() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vf_.vkDestroyCommandPool(device_, commandPool_, nullptr);
}

} // namespace igl::vulkan
