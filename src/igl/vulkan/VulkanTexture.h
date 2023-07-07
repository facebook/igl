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

namespace igl {
namespace vulkan {

class VulkanContext;
class VulkanImage;
class VulkanImageView;

class VulkanTexture final {
 public:
  VulkanTexture(const VulkanContext& ctx,
                std::shared_ptr<VulkanImage> image,
                std::shared_ptr<VulkanImageView> imageView);
  ~VulkanTexture();

  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

  VulkanImage& getVulkanImage() {
    return *image_.get();
  }
  const VulkanImage& getVulkanImage() const {
    return *image_.get();
  }
  VulkanImageView& getVulkanImageView() {
    return *imageView_.get();
  }
  const VulkanImageView& getVulkanImageView() const {
    return *imageView_.get();
  }
  uint32_t getTextureId() const {
    return textureId_;
  }

 private:
  friend class VulkanContext;
  const VulkanContext& ctx_;
  std::shared_ptr<VulkanImage> image_;
  std::shared_ptr<VulkanImageView> imageView_;
  // an index into VulkanContext::textures_
  uint32_t textureId_ = 0;
};

} // namespace vulkan
} // namespace igl
