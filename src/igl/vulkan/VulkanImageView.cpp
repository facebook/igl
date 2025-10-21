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
                                 const VulkanImageViewCreateInfo& ci,
                                 const char* debugName) :
  VulkanImageView(ctx,
                  VkImageViewCreateInfo{
                      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                      .pNext = nullptr,
                      .flags = 0,
                      .image = ci.image,
                      .viewType = ci.viewType,
                      .format = ci.format,
                      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                      .subresourceRange = ci.subresourceRange,
                  },
                  debugName) {}

VulkanImageView::~VulkanImageView() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  destroy();
}

VulkanImageView::VulkanImageView(const VulkanContext& ctx,
                                 const VkImageViewCreateInfo& ci,
                                 const char* debugName) :
  ctx_(&ctx), aspectMask_(ci.subresourceRange.aspectMask) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkImageViewCreateInfo ciCopy(ci);

  VkSamplerYcbcrConversionInfo info{};

  if (!ci.pNext && igl::vulkan::getNumImagePlanes(ci.format) > 1) {
    info = ctx.getOrCreateYcbcrConversionInfo(ci.format);
    ciCopy.pNext = &info;
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
