/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImageView.h"

#include <igl/vulkan/VulkanContext.h>
#include <lvk/vulkan/VulkanUtils.h>

namespace lvk {

namespace vulkan {

VulkanImageView::VulkanImageView(const VulkanContext& ctx,
                                 VkDevice device,
                                 VkImage image,
                                 VkImageViewType type,
                                 VkFormat format,
                                 VkImageAspectFlags aspectMask,
                                 uint32_t baseLevel,
                                 uint32_t numLevels,
                                 uint32_t baseLayer,
                                 uint32_t numLayers,
                                 const char* debugName) :
  ctx_(ctx), device_(device) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  const VkImageViewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = type,
      .format = format,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {aspectMask, baseLevel, numLevels, baseLayer, numLayers},
  };
  VK_ASSERT(vkCreateImageView(device, &ci, nullptr, &vkImageView_));
  VK_ASSERT(lvk::setDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkImageView_, debugName));
}

VulkanImageView::~VulkanImageView() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  ctx_.deferredTask(
      std::packaged_task<void()>([device = device_, imageView = vkImageView_]() { vkDestroyImageView(device, imageView, nullptr); }));
}

} // namespace vulkan

} // namespace lvk
