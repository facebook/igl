/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImageView.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>

namespace igl {

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
  ctx_(&ctx), device_(device) {
  IGL_ASSERT(ctx_);
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(ivkCreateImageView(
      &ctx_->vf_,
      device_,
      image,
      type,
      format,
      VkImageSubresourceRange{aspectMask, baseLevel, numLevels, baseLayer, numLayers},
      &vkImageView_));

  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device_, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkImageView_, debugName));
}

VulkanImageView::VulkanImageView(const VulkanContext& ctx,
                                 VkDevice device,
                                 VkImage image,
                                 const VulkanImageViewCreateInfo& createInfo,
                                 const char* debugName) :
  VulkanImageView(ctx,
                  device,
                  image,
                  createInfo.type,
                  createInfo.format,
                  createInfo.aspectMask,
                  createInfo.baseLevel,
                  createInfo.numLevels,
                  createInfo.baseLayer,
                  createInfo.numLayers,
                  debugName) {}

VulkanImageView::~VulkanImageView() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  destroy();
}

void VulkanImageView::destroy() {
  if (ctx_) {
    ctx_->deferredTask(
        std::packaged_task<void()>([vf = &ctx_->vf_, device = device_, imageView = vkImageView_]() {
          vf->vkDestroyImageView(device, imageView, nullptr);
        }));

    vkImageView_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    ctx_ = nullptr;
  }
}

} // namespace vulkan

} // namespace igl
