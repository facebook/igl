/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanDescriptorSetLayout.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanPipelineLayout.h>

namespace lvk {

namespace vulkan {

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VkDevice device,
                                                     uint32_t numBindings,
                                                     const VkDescriptorSetLayoutBinding* bindings,
                                                     const VkDescriptorBindingFlags* bindingFlags,
                                                     const char* debugName) :
  device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateDescriptorSetLayout(
      device, numBindings, bindings, bindingFlags, &vkDescriptorSetLayout_));
  VK_ASSERT(ivkSetDebugObjectName(
      device_, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)vkDescriptorSetLayout_, debugName));
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vkDestroyDescriptorSetLayout(device_, vkDescriptorSetLayout_, nullptr);
}

} // namespace vulkan

} // namespace lvk
