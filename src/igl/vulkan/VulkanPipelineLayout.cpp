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

VulkanPipelineLayout::VulkanPipelineLayout(const VulkanFunctionTable& vf,
                                           VkDevice device,
                                           const VkDescriptorSetLayout* layouts,
                                           uint32_t numLayouts,
                                           const VkPushConstantRange& range,
                                           const char* debugName) :
  vf_(vf), device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkPipelineLayoutCreateInfo ci = ivkGetPipelineLayoutCreateInfo(numLayouts, layouts, &range);

  VK_ASSERT(vf_.vkCreatePipelineLayout(device, &ci, nullptr, &vkPipelineLayout_));
  VK_ASSERT(ivkSetDebugObjectName(
      &vf_, device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)vkPipelineLayout_, debugName));
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vf_.vkDestroyPipelineLayout(device_, vkPipelineLayout_, nullptr);
}

} // namespace vulkan

} // namespace igl
