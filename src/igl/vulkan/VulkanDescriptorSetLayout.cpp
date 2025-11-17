/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanDescriptorSetLayout.h"

#include <igl/vulkan/Common.h>

namespace igl::vulkan {

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(const VulkanContext& ctx,
                                                     VkDescriptorSetLayoutCreateFlags flags,
                                                     uint32_t numBindings,
                                                     const VkDescriptorSetLayoutBinding* bindings,
                                                     const VkDescriptorBindingFlags* bindingFlags,
                                                     const char* debugName) :
  ctx(ctx), numBindings(numBindings) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateDescriptorSetLayout(&ctx.vf_,
                                         ctx.getVkDevice(),
                                         flags,
                                         numBindings,
                                         bindings,
                                         bindingFlags,
                                         &vkDescriptorSetLayout));
  VK_ASSERT(ivkSetDebugObjectName(&ctx.vf_,
                                  ctx.getVkDevice(),
                                  VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                  (uint64_t)vkDescriptorSetLayout,
                                  debugName));
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  ctx.freeResourcesForDescriptorSetLayout(vkDescriptorSetLayout);
  ctx.deferredTask(std::packaged_task<void()>(
      [vf = &ctx.vf_, device = ctx.getVkDevice(), layout = vkDescriptorSetLayout] {
        vf->vkDestroyDescriptorSetLayout(device, layout, nullptr);
      }));
}

} // namespace igl::vulkan
