/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanTexture.h"

#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <lvk/vulkan/VulkanUtils.h>

namespace lvk::vulkan {

VulkanTexture::VulkanTexture(std::shared_ptr<VulkanImage> image, VkImageView imageView) : image_(std::move(image)), imageView_(imageView) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT(image_.get());
  LVK_ASSERT(imageView_ != VK_NULL_HANDLE);
}

VulkanTexture::~VulkanTexture() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (image_) {
    image_->ctx_.deferredTask(std::packaged_task<void()>(
        [device = image_->ctx_.getVkDevice(), imageView = imageView_]() { vkDestroyImageView(device, imageView, nullptr); }));
    for (VkImageView v : imageViewForFramebuffer_) {
      if (v != VK_NULL_HANDLE) {
        image_->ctx_.deferredTask(std::packaged_task<void()>(
            [device = image_->ctx_.getVkDevice(), imageView = v]() { vkDestroyImageView(device, imageView, nullptr); }));
      }
    }
  }
}

VulkanTexture::VulkanTexture(VulkanTexture&& other) {
  std::swap(image_, other.image_);
  std::swap(imageView_, other.imageView_);
  std::swap(imageViewForFramebuffer_, other.imageViewForFramebuffer_);
}

VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other) {
  std::swap(image_, other.image_);
  std::swap(imageView_, other.imageView_);
  std::swap(imageViewForFramebuffer_, other.imageViewForFramebuffer_);
  return *this;
}

bool VulkanTexture::isSwapchainTexture() const {
  return image_->isExternallyManaged_;
}

VkImageView VulkanTexture::getVkImageView() const {
  return imageView_;
}

VkImageView VulkanTexture::getVkImageViewForFramebuffer(uint8_t level) {
  LVK_ASSERT(level < LVK_MAX_MIP_LEVELS);

  if (level >= LVK_MAX_MIP_LEVELS) {
    return VK_NULL_HANDLE;
  }

  if (imageViewForFramebuffer_[level] != VK_NULL_HANDLE) {
    return imageViewForFramebuffer_[level];
  }

  imageViewForFramebuffer_[level] =
      image_->createImageView(VK_IMAGE_VIEW_TYPE_2D, image_->imageFormat_, image_->getImageAspectFlags(), level, 1u, 0u, 1u);

  return imageViewForFramebuffer_[level];
}

Dimensions VulkanTexture::getDimensions() const {
  LVK_ASSERT(image_.get());

  return {
      .width = image_->extent_.width,
      .height = image_->extent_.height,
      .depth = image_->extent_.depth,
  };
}

} // namespace lvk::vulkan
