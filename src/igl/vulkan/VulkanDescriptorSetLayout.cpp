/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanDescriptorSetLayout.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanPipelineLayout.h>

namespace igl::vulkan {

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(const VulkanFunctionTable& vf,
                                                     VkDevice device,
                                                     VkDescriptorSetLayoutCreateFlags flags,
                                                     uint32_t numBindings,
                                                     const VkDescriptorSetLayoutBinding* bindings,
                                                     const VkDescriptorBindingFlags* bindingFlags,
                                                     const char* debugName) :
  vf_(vf), device_(device), numBindings_(numBindings) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateDescriptorSetLayout(
      &vf, device, flags, numBindings, bindings, bindingFlags, &vkDescriptorSetLayout_));
  VK_ASSERT(ivkSetDebugObjectName(&vf,
                                  device_,
                                  VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                  (uint64_t)vkDescriptorSetLayout_,
                                  debugName));
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vf_.vkDestroyDescriptorSetLayout(device_, vkDescriptorSetLayout_, nullptr);
}

} // namespace igl::vulkan
