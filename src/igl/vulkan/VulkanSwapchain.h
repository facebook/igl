/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFence.h>
#include <igl/vulkan/VulkanFramebuffer.h>
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
  VulkanSwapchain(const VulkanContext& ctx, uint32_t width, uint32_t height);
  ~VulkanSwapchain();

  Result acquireNextImage();
  Result present(VkSemaphore waitSemaphore);
  VkImage getCurrentVkImage() const {
    if (IGL_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
      return swapchainTextures_[currentImageIndex_]->getVulkanImage().getVkImage();
    }
    return VK_NULL_HANDLE;
  }
  VkImageView getCurrentVkImageView() const {
    if (IGL_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
      return swapchainTextures_[currentImageIndex_]->getVulkanImageView().getVkImageView();
    }
    return VK_NULL_HANDLE;
  }

  std::shared_ptr<VulkanTexture> getCurrentDepthTexture() {
    if (!depthTexture_) {
      lazyAllocateDepthBuffer();
    }

    return depthTexture_;
  }

  std::shared_ptr<VulkanTexture> getCurrentVulkanTexture() {
    if (getNextImage_) {
      acquireNextImage();
      getNextImage_ = false;
    }

    if (IGL_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
      return swapchainTextures_[currentImageIndex_];
    }

    return nullptr;
  }

  VkImage getDepthVkImage() const;
  VkImageView getDepthVkImageView() const;
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

  [[nodiscard]] VkSemaphore getSemaphore() const noexcept;

  uint64_t getFrameNumber() const {
    return frameNumber_;
  }

 private:
  void lazyAllocateDepthBuffer() const;

 private:
  std::vector<VulkanSemaphore> acquireSemaphores_;
  // Used to check whether the acquire semaphore can be used for acquiring
  // Based on
  // https://github.com/corporateshark/lightweightvk/blob/36597fae5c79ad6b310e4ed8c00e8acfa38b5aca/lvk/vulkan/VulkanClasses.h#L146
  std::vector<VulkanFence> acquireFences_;

 private:
  const VulkanContext& ctx_;
  VkDevice device_;
  VkQueue graphicsQueue_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t numSwapchainImages_ = 0;
  uint32_t currentImageIndex_ = 0;
  // Because the next acquired image's index is obtained _after_ requesting it (along with
  // semaphores and fences), the index of the semaphore and fence used to synchronize the current
  // swapchain image is different than the `currentImageIndex_`
  uint32_t currentSemaphoreIndex_ = 0;
  uint64_t frameNumber_ = 0;
  bool getNextImage_ = true;
  VkSwapchainKHR swapchain_{};
  std::unique_ptr<std::shared_ptr<VulkanTexture>[]> swapchainTextures_;
  mutable std::shared_ptr<VulkanTexture> depthTexture_;
  VkSurfaceFormatKHR surfaceFormat_{};
};

} // namespace igl::vulkan
