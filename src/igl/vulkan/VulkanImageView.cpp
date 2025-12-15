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
                      .components = ci.components,
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
  ctx(&ctx), aspectMask(ci.subresourceRange.aspectMask) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkImageViewCreateInfo ciCopy(ci);

  VkSamplerYcbcrConversionInfo info{};

  if (!ci.pNext && igl::vulkan::getNumImagePlanes(ci.format) > 1) {
    info = ctx.getOrCreateYcbcrConversionInfo(ci.format);
    ciCopy.pNext = &info;
  }

  VkDevice device = this->ctx->getVkDevice();
  VK_ASSERT(this->ctx->vf_.vkCreateImageView(device, &ci, nullptr, &vkImageView));

  VK_ASSERT(ivkSetDebugObjectName(
      &this->ctx->vf_, device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkImageView, debugName));
}

VulkanImageView& VulkanImageView::operator=(VulkanImageView&& other) noexcept {
  destroy();
  ctx = other.ctx;
  vkImageView = other.vkImageView;
  aspectMask = other.aspectMask;
  other.ctx = nullptr;
  other.vkImageView = VK_NULL_HANDLE;
  other.aspectMask = 0;
  return *this;
}

[[nodiscard]] bool VulkanImageView::valid() const {
  return ctx != nullptr;
}

void VulkanImageView::destroy() {
  if (!valid()) {
    return;
  }

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx);

  ctx->deferredTask(std::packaged_task<void()>(
      [vf = &ctx->vf_, device = ctx->getVkDevice(), imageView = vkImageView]() {
        vf->vkDestroyImageView(device, imageView, nullptr);
      }));

  vkImageView = VK_NULL_HANDLE;
  ctx = nullptr;
}

} // namespace igl::vulkan
