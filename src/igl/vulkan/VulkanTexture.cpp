/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanTexture.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>

namespace lvk::vulkan {

VulkanTexture::VulkanTexture(std::shared_ptr<VulkanImage> image, std::shared_ptr<VulkanImageView> imageView) :
  image_(std::move(image)), imageView_(std::move(imageView)) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT(image_.get());
  IGL_ASSERT(imageView_.get());
}

bool VulkanTexture::isSwapchainTexture() const {
  return image_->isExternallyManaged_;
}

VkImageView VulkanTexture::getVkImageView() const {
  return imageView_->vkImageView_;
}

VkImageView VulkanTexture::getVkImageViewForFramebuffer(uint32_t level) const {
  if (level < imageViewForFramebuffer_.size() && imageViewForFramebuffer_[level]) {
    return imageViewForFramebuffer_[level]->getVkImageView();
  }

  if (level >= imageViewForFramebuffer_.size()) {
    imageViewForFramebuffer_.resize(level + 1);
  }

  const VkImageAspectFlags flags = image_->getImageAspectFlags();

  imageViewForFramebuffer_[level] = image_->createImageView(VK_IMAGE_VIEW_TYPE_2D, image_->imageFormat_, flags, level, 1u, 0u, 1u);

  return imageViewForFramebuffer_[level]->vkImageView_;
}

Dimensions VulkanTexture::getDimensions() const {
  return {
      .width = image_->extent_.width,
      .height = image_->extent_.height,
      .depth = image_->extent_.depth,
  };
}

} // namespace lvk::vulkan
