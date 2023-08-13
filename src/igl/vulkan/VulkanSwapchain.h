/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanTexture.h>
#include <vector>

namespace lvk::vulkan {

class VulkanContext;
class VulktanTexture;

class VulkanSwapchain final {
 public:
  VulkanSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height);
  ~VulkanSwapchain();

  Result present(VkSemaphore waitSemaphore);
  VkImage getCurrentVkImage() const;
  VkImageView getCurrentVkImageView() const;
  TextureHandle getCurrentTexture();

  uint32_t getWidth() const {
    return width_;
  }
  uint32_t getHeight() const {
    return height_;
  }
  VkExtent2D getExtent() const {
    return VkExtent2D{width_, height_};
  }

  VkFormat getFormatColor() const {
    return surfaceFormat_.format;
  }

  uint32_t getNumSwapchainImages() const {
    return numSwapchainImages_;
  }

  uint32_t getCurrentImageIndex() const {
    return currentImageIndex_;
  }
 public:
  VkSemaphore acquireSemaphore_ = VK_NULL_HANDLE;

 private:
  VulkanContext& ctx_;
  VkDevice device_;
  VkQueue graphicsQueue_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t numSwapchainImages_ = 0;
  uint32_t currentImageIndex_ = 0;
  bool getNextImage_ = true;
  VkSwapchainKHR swapchain_;
  std::vector<TextureHandle> swapchainTextures_;
  VkSurfaceFormatKHR surfaceFormat_;
};

} // namespace lvk::vulkan
