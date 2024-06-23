/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

class VulkanContext;

/// @brief A wrapper around a Vulkan Framebuffer object
class VulkanFramebuffer final {
 public:
  /// @brief Constructs a VulkanFramebuffer object with the parameters provided and an optional
  /// debug name
  VulkanFramebuffer(const VulkanContext& ctx,
                    VkDevice device,
                    uint32_t width,
                    uint32_t height,
                    VkRenderPass renderPass,
                    size_t numAttachments,
                    const VkImageView* attachments,
                    const char* debugName = nullptr);

  /// @brief Queues the destruction of the framebuffer on the Vulkan context via a deferred task.
  /// For more details about deferred tasks, please refer to the igl::vulkan::VulkanContext class
  ~VulkanFramebuffer();

  VulkanFramebuffer(const VulkanFramebuffer&) = delete;
  VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

  [[nodiscard]] VkFramebuffer getVkFramebuffer() const {
    return vkFramebuffer_;
  }

 public:
  const VulkanContext& ctx_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkFramebuffer vkFramebuffer_ = VK_NULL_HANDLE;
};

} // namespace igl::vulkan
