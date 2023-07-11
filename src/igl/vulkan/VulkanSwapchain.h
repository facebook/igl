/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanTexture.h>
#include <vector>

namespace igl::vulkan {

class VulkanContext;
class VulkanSemaphore;
class VulktanTexture;

class VulkanSwapchain final {
 public:
  VulkanSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height);
  ~VulkanSwapchain();

  Result present(VkSemaphore waitSemaphore);
  VkImage getCurrentVkImage() const {
    if (IGL_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
      return swapchainTextures_[currentImageIndex_]
          ->getVulkanTexture()
          .getVulkanImage()
          .getVkImage();
    }
    return VK_NULL_HANDLE;
  }
  VkImageView getCurrentVkImageView() const {
    if (IGL_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
      return swapchainTextures_[currentImageIndex_]
          ->getVulkanTexture()
          .getVulkanImageView()
          .getVkImageView();
    }
    return VK_NULL_HANDLE;
  }
  std::shared_ptr<Texture> getCurrentTexture();

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

  uint64_t getFrameNumber() const {
    return frameNumber_;
  }

 public:
  std::unique_ptr<igl::vulkan::VulkanSemaphore> acquireSemaphore_;

 private:
  VulkanContext& ctx_;
  VkDevice device_;
  VkQueue graphicsQueue_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t numSwapchainImages_ = 0;
  uint32_t currentImageIndex_ = 0;
  uint64_t frameNumber_ = 0;
  bool getNextImage_ = true;
  VkSwapchainKHR swapchain_;
  std::vector<std::shared_ptr<Texture>> swapchainTextures_;
  VkSurfaceFormatKHR surfaceFormat_;
};

} // namespace igl::vulkan
