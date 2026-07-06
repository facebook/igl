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

  // The underlying VkDescriptorSetLayout is de-duplicated and owned by VulkanContext. Multiple
  // VulkanDescriptorSetLayout instances with identical bindings/flags will therefore share the
  // same handle, and consequently share the same DescriptorPoolsArena in VulkanContext.
  vkDescriptorSetLayout = ctx.getOrCreateVkDescriptorSetLayout(
      flags, numBindings, bindings, bindingFlags, debugName);

  if (ctx.features().has_VK_EXT_descriptor_buffer) {
    ctx.vf_.vkGetDescriptorSetLayoutSizeEXT(ctx.getVkDevice(), vkDescriptorSetLayout, &layoutSize);
  }
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  // The VkDescriptorSetLayout handle is owned and destroyed by VulkanContext (see dslCache in
  // VulkanContextImpl). Nothing to do here.
}

} // namespace igl::vulkan
