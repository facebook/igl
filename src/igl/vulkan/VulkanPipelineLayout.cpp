/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanPipelineLayout.h"

#include <igl/vulkan/Common.h>

namespace igl {

namespace vulkan {

VulkanPipelineLayout::VulkanPipelineLayout(VkDevice device,
                                           VkDescriptorSetLayout layout,
                                           const VkPushConstantRange& range,
                                           const char* debugName) :
  device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkPipelineLayoutCreateInfo ci = ivkGetPipelineLayoutCreateInfo(1, &layout, &range);

  VK_ASSERT(vkCreatePipelineLayout(device, &ci, nullptr, &vkPipelineLayout_));
  VK_ASSERT(ivkSetDebugObjectName(
      device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)vkPipelineLayout_, debugName));
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vkDestroyPipelineLayout(device_, vkPipelineLayout_, nullptr);
}

} // namespace vulkan

} // namespace igl
