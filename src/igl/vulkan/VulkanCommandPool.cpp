/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanCommandPool.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl {
namespace vulkan {

VulkanCommandPool::VulkanCommandPool(VkDevice device,
                                     VkCommandPoolCreateFlags flags,
                                     uint32_t queueFamilyIndex,
                                     const char* debugName) :
  device_(device), queueFamilyIndex_(queueFamilyIndex) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateCommandPool(device_, flags, queueFamilyIndex_, &commandPool_));

  ivkSetDebugObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)commandPool_, debugName);
}

VulkanCommandPool ::~VulkanCommandPool() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vkDestroyCommandPool(device_, commandPool_, nullptr);
}

} // namespace vulkan
} // namespace igl
