/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanFramebuffer.h"

#include "VulkanContext.h"

namespace igl::vulkan {
VulkanFramebuffer::VulkanFramebuffer(const VulkanContext& ctx,
                                     VkDevice device,
                                     uint32_t width,
                                     uint32_t height,
                                     VkRenderPass renderPass,
                                     size_t numAttachments,
                                     const VkImageView* attachments,
                                     const char* debugName) :
  ctx_(ctx), device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  if (!IGL_DEBUG_VERIFY(renderPass != VK_NULL_HANDLE)) {
    return;
  }
  if (!IGL_DEBUG_VERIFY(attachments)) {
    return;
  }

  VK_ASSERT(ivkCreateFramebuffer(
      &ctx_.vf_, device_, width, height, renderPass, numAttachments, attachments, &vkFramebuffer_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_.vf_, device_, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)vkFramebuffer_, debugName));
}

VulkanFramebuffer::~VulkanFramebuffer() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  ctx_.deferredTask(std::packaged_task<void()>(
      [vf = &ctx_.vf_, device = device_, framebuffer = vkFramebuffer_]() {
        vf->vkDestroyFramebuffer(device, framebuffer, nullptr);
      }));
}

} // namespace igl::vulkan
