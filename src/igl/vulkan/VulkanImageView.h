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
  VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkImageAspectFlags aspectMask = 0;
  uint32_t baseLevel = 0;
  uint32_t numLevels = 1;
  uint32_t baseLayer = 0;
  uint32_t numLayers = 1;
};

/**
 * @brief VulkanImageView is a RAII wrapper for VkImageView.
 * The device member is not managed by this class (it is used to destroy the imageView).
 */
class VulkanImageView final {
 public:
  explicit VulkanImageView() = default;
  /**
   * @brief Creates the VulkanImageView object which stores a handle to a VkImageView.
   * The imageView is created from the device, image, and other parameters with a name that can be
   * used for debugging.
   */
  VulkanImageView(const VulkanContext& ctx,
                  VkImage image,
                  VkImageViewType type,
                  VkFormat format,
                  VkImageAspectFlags aspectMask,
                  uint32_t baseLevel,
                  uint32_t numLevels,
                  uint32_t baseLayer,
                  uint32_t numLayers,
                  const char* debugName = nullptr);

  /**
   * @brief Creates the VulkanImageView object which stores a handle to a VkImageView.
   * The imageView is created from the device, image, and other parameters with a name that can be
   * used for debugging.
   */
  VulkanImageView(const VulkanContext& ctx,
                  VkDevice device,
                  VkImage image,
                  const VulkanImageViewCreateInfo& createInfo,
                  const char* debugName = nullptr);

  VulkanImageView(const VulkanContext& ctx,
                  const VkImageViewCreateInfo& createInfo,
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
    return vkImageView_;
  }
  /**
   * @brief Returns true if the object is valid
   */
  [[nodiscard]] bool valid() const;
  /**
   * @brief Returns the VkImageAspectFlags used to create the imageView
   */
  [[nodiscard]] VkImageAspectFlags getVkImageAspectFlags() const {
    return aspectMask_;
  }

 public:
  const VulkanContext* ctx_ = nullptr;
  VkImageView vkImageView_ = VK_NULL_HANDLE;
  VkImageAspectFlags aspectMask_ = 0;

 private:
  void destroy();
};

} // namespace igl::vulkan
