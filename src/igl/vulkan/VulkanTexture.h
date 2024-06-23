/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <igl/ColorSpace.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>

namespace igl::vulkan {

class VulkanContext;

class VulkanTexture final {
 public:
  VulkanTexture(VulkanImage&& image, VulkanImageView&& imageView);
  ~VulkanTexture() = default;

  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

  const VulkanImage& getVulkanImage() const {
    return image_;
  }
  const VulkanImageView& getVulkanImageView() const {
    return imageView_;
  }
  uint32_t getTextureId() const {
    return textureId_;
  }

 private:
  friend class VulkanContext;
  VulkanImage image_;
  VulkanImageView imageView_;
  // an index into VulkanContext::textures_
  uint32_t textureId_ = 0;
};

} // namespace igl::vulkan
