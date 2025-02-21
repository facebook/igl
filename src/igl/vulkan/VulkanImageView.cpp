/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImageView.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

VulkanImageView::VulkanImageView(const VulkanContext& ctx,
                                 VkImage image,
                                 VkImageViewType type,
                                 VkFormat format,
                                 VkImageAspectFlags aspectMask,
                                 uint32_t baseLevel,
                                 uint32_t numLevels,
                                 uint32_t baseLayer,
                                 uint32_t numLayers,
                                 const char* debugName) :
  ctx_(&ctx), aspectMask_(aspectMask) {
  IGL_DEBUG_ASSERT(ctx_);
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkDevice device = ctx_->getVkDevice();

  VkImageViewCreateInfo ci = ivkGetImageViewCreateInfo(
      image,
      type,
      format,
      VkImageSubresourceRange{aspectMask, baseLevel, numLevels, baseLayer, numLayers});

  VkSamplerYcbcrConversionInfo info{};

  if (igl::vulkan::getNumImagePlanes(format) > 1) {
    info = ctx.getOrCreateYcbcrConversionInfo(format);
    ci.pNext = &info;
  }

  VK_ASSERT(ctx_->vf_.vkCreateImageView(device, &ci, nullptr, &vkImageView_));

  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkImageView_, debugName));
}

VulkanImageView::VulkanImageView(const VulkanContext& ctx,
                                 VkDevice /*device*/,
                                 VkImage image,
                                 const VulkanImageViewCreateInfo& createInfo,
                                 const char* debugName) :
  VulkanImageView(ctx,
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

VulkanImageView::VulkanImageView(const VulkanContext& ctx,
                                 const VkImageViewCreateInfo& createInfo,
                                 const char* debugName) :
  ctx_(&ctx), aspectMask_(createInfo.subresourceRange.aspectMask) {
  VkDevice device = ctx_->getVkDevice();
  VK_ASSERT(ctx_->vf_.vkCreateImageView(device, &createInfo, nullptr, &vkImageView_));

  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkImageView_, debugName));
}

VulkanImageView& VulkanImageView::operator=(VulkanImageView&& other) noexcept {
  destroy();
  ctx_ = other.ctx_;
  vkImageView_ = other.vkImageView_;
  aspectMask_ = other.aspectMask_;
  other.ctx_ = nullptr;
  other.vkImageView_ = VK_NULL_HANDLE;
  other.aspectMask_ = 0;
  return *this;
}

[[nodiscard]] bool VulkanImageView::valid() const {
  return ctx_ != nullptr;
}

void VulkanImageView::destroy() {
  if (!valid()) {
    return;
  }

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  ctx_->deferredTask(std::packaged_task<void()>(
      [vf = &ctx_->vf_, device = ctx_->getVkDevice(), imageView = vkImageView_]() {
        vf->vkDestroyImageView(device, imageView, nullptr);
      }));

  vkImageView_ = VK_NULL_HANDLE;
  ctx_ = nullptr;
}

} // namespace igl::vulkan
