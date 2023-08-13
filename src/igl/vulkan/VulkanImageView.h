/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <lvk/vulkan/VulkanUtils.h>

namespace lvk {
namespace vulkan {

class VulkanContext;

/**
 * @brief VulkanImageView is a RAII wrapper for VkImageView.
 * The device member is not managed by this class (it is used to destroy the imageView).
 */
class VulkanImageView final {
 public:
  /**
   * @brief Creates the VulkanImageView object which stores a handle to a VkImageView.
   * The imageView is created from the device, image, and other parameters with a name that can be
   * used for debugging.
   */
  VulkanImageView(const VulkanContext& ctx,
                  VkDevice device,
                  VkImage image,
                  VkImageViewType type,
                  VkFormat format,
                  VkImageAspectFlags aspectMask,
                  uint32_t baseLevel,
                  uint32_t numLevels,
                  uint32_t baseLayer,
                  uint32_t numLayers,
                  const char* debugName = nullptr);
  ~VulkanImageView();

  VulkanImageView(const VulkanImageView&) = delete;
  VulkanImageView& operator=(const VulkanImageView&) = delete;

  /**
   * @brief Returns Vulkan's opaque handle to the imageView object
   */
  VkImageView getVkImageView() const {
    return vkImageView_;
  }

 public:
  const VulkanContext& ctx_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkImageView vkImageView_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace lvk
