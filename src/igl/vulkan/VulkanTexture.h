/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <lvk/vulkan/VulkanUtils.h>

namespace lvk::vulkan {

class VulkanImage;

class VulkanTexture final {
 public:
  VulkanTexture() = default;
  VulkanTexture(std::shared_ptr<VulkanImage> image, VkImageView imageView);
  ~VulkanTexture();


  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

  VulkanTexture(VulkanTexture&& other);
  VulkanTexture& operator=(VulkanTexture&& other);

  Dimensions getDimensions() const;
  VkImageView getVkImageView() const; // all mip-levels
  VkImageView getVkImageViewForFramebuffer(uint8_t level); // framebuffers can render only into 1 mip-level
  bool isSwapchainTexture() const;

 public:
  std::shared_ptr<VulkanImage> image_;
  VkImageView imageView_ = VK_NULL_HANDLE;
  VkImageView imageViewForFramebuffer_[LVK_MAX_MIP_LEVELS] = {};
};

} // namespace lvk::vulkan
