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

  const bool hasYcbcrConversion = igl::vulkan::getNumImagePlanes(format) > 1;

  VkSamplerYcbcrConversionInfo info{};

  if (hasYcbcrConversion) {
    info = ctx.getOrCreateYcbcrConversionInfo(format);
  }

  const VkImageViewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = hasYcbcrConversion ? &info : nullptr,
      .flags = 0,
      .image = image,
      .viewType = type,
      .format = format,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = VkImageSubresourceRange{.aspectMask = aspectMask,
                                                  .baseMipLevel = baseLevel,
                                                  .levelCount = numLevels,
                                                  .baseArrayLayer = baseLayer,
                                                  .layerCount = numLayers},
  };

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
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkImageViewCreateInfo ci(createInfo);

  VkSamplerYcbcrConversionInfo info{};

  if (!ci.pNext && igl::vulkan::getNumImagePlanes(ci.format) > 1) {
    info = ctx.getOrCreateYcbcrConversionInfo(ci.format);
    ci.pNext = &info;
  }

  VkDevice device = ctx_->getVkDevice();
  VK_ASSERT(ctx_->vf_.vkCreateImageView(device, &ci, nullptr, &vkImageView_));

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
