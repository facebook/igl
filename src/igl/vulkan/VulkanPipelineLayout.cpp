/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanPipelineLayout.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFunctionTable.h>

namespace igl::vulkan {

VulkanPipelineLayout::VulkanPipelineLayout(const VulkanContext& ctx,
                                           VkDevice device,
                                           const VkDescriptorSetLayout* layouts,
                                           uint32_t numLayouts,
                                           const VkPushConstantRange& range,
                                           const char* debugName) :
  ctx_(ctx), device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  const VkPipelineLayoutCreateInfo ci = ivkGetPipelineLayoutCreateInfo(numLayouts, layouts, &range);

  VK_ASSERT(ctx.vf_.vkCreatePipelineLayout(device, &ci, nullptr, &vkPipelineLayout_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx.vf_, device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)vkPipelineLayout_, debugName));
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  ctx_.deferredTask(std::packaged_task<void()>(
      [vf = &ctx_.vf_, device = ctx_.getVkDevice(), layout = vkPipelineLayout_]() {
        vf->vkDestroyPipelineLayout(device, layout, nullptr);
      }));
}

} // namespace igl::vulkan
