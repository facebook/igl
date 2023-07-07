/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl {
namespace vulkan {

class VulkanContext;

class VulkanFramebuffer final {
 public:
  VulkanFramebuffer(const VulkanContext& ctx,
                    VkDevice device,
                    uint32_t width,
                    uint32_t height,
                    VkRenderPass renderPass,
                    size_t numAttachments,
                    const VkImageView* attachments,
                    const char* debugName = nullptr);
  ~VulkanFramebuffer();

  VulkanFramebuffer(const VulkanFramebuffer&) = delete;
  VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

  VkFramebuffer getVkFramebuffer() const {
    return vkFramebuffer_;
  }

 public:
  const VulkanContext& ctx_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkFramebuffer vkFramebuffer_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
