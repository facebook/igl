/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace lvk::vulkan {

class VulkanImage;
class VulkanImageView;

class VulkanTexture final {
 public:
  VulkanTexture() = default;
  VulkanTexture(std::shared_ptr<VulkanImage> image, std::shared_ptr<VulkanImageView> imageView);

  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

  VulkanTexture(VulkanTexture&& other) = default;
  VulkanTexture& operator=(VulkanTexture&& other) = default;

  Dimensions getDimensions() const;
  VkImageView getVkImageView() const; // all mip-levels
  VkImageView getVkImageViewForFramebuffer(uint32_t level) const; // framebuffers can render only
                                                                  // into 1 mip-level
  bool isSwapchainTexture() const;

 public:
  std::shared_ptr<VulkanImage> image_;
  std::shared_ptr<VulkanImageView> imageView_;
  mutable std::vector<std::shared_ptr<VulkanImageView>> imageViewForFramebuffer_;
};

} // namespace lvk::vulkan
