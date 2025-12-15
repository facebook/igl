/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

class VulkanContext;

struct VulkanImageViewCreateInfo {
  VkImage image = VK_NULL_HANDLE;
  VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkComponentMapping components = {
      .r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .a = VK_COMPONENT_SWIZZLE_IDENTITY,
  };
  VkImageSubresourceRange subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };
};

/**
 * @brief VulkanImageView is a RAII wrapper for VkImageView.
 * The device member is not managed by this class (it is used to destroy the imageView).
 */
class VulkanImageView final {
 public:
  explicit VulkanImageView() = default;
  /**
   * @brief Creates the VulkanImageView object which stores a handle to a newly created VkImageView.
   * The imageView is created from the device, image, and other parameters with a name that can be
   * used for debugging.
   */
  VulkanImageView(const VulkanContext& ctx,
                  const VulkanImageViewCreateInfo& ci,
                  const char* debugName = nullptr);

  VulkanImageView(const VulkanContext& ctx,
                  const VkImageViewCreateInfo& ci,
                  const char* debugName = nullptr);

  ~VulkanImageView();

  VulkanImageView(const VulkanImageView&) = delete;
  VulkanImageView& operator=(const VulkanImageView&) = delete;

  VulkanImageView(VulkanImageView&& other) noexcept {
    *this = std::move(other);
  }
  VulkanImageView& operator=(VulkanImageView&& other) noexcept;

  /**
   * @brief Returns Vulkan's opaque handle to the imageView object
   */
  [[nodiscard]] VkImageView getVkImageView() const {
    return vkImageView;
  }
  /**
   * @brief Returns true if the object is valid
   */
  [[nodiscard]] bool valid() const;
  /**
   * @brief Returns the VkImageAspectFlags used to create the imageView
   */
  [[nodiscard]] VkImageAspectFlags getVkImageAspectFlags() const {
    return aspectMask;
  }

 public:
  const VulkanContext* ctx = nullptr;
  VkImageView vkImageView = VK_NULL_HANDLE;
  VkImageAspectFlags aspectMask = 0;

 private:
  void destroy();
};

} // namespace igl::vulkan
