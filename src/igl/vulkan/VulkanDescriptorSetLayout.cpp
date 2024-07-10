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

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(const VulkanContext& ctx,
                                                     VkDescriptorSetLayoutCreateFlags flags,
                                                     uint32_t numBindings,
                                                     const VkDescriptorSetLayoutBinding* bindings,
                                                     const VkDescriptorBindingFlags* bindingFlags,
                                                     const char* debugName) :
  ctx_(ctx), numBindings_(numBindings) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateDescriptorSetLayout(&ctx.vf_,
                                         ctx.getVkDevice(),
                                         flags,
                                         numBindings,
                                         bindings,
                                         bindingFlags,
                                         &vkDescriptorSetLayout_));
  VK_ASSERT(ivkSetDebugObjectName(&ctx.vf_,
                                  ctx.getVkDevice(),
                                  VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                  (uint64_t)vkDescriptorSetLayout_,
                                  debugName));
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  ctx_.freeResourcesForDescriptorSetLayout(vkDescriptorSetLayout_);
  ctx_.deferredTask(std::packaged_task<void()>(
      [vf = &ctx_.vf_, device = ctx_.getVkDevice(), layout = vkDescriptorSetLayout_] {
        vf->vkDestroyDescriptorSetLayout(device, layout, nullptr);
      }));
}

} // namespace igl::vulkan
