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

namespace igl::vulkan {

class VulkanTexture final {
 public:
  VulkanTexture(VulkanImage&& image, VulkanImageView&& imageView);
  ~VulkanTexture() = default;

  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

 public:
  VulkanImage image_;
  VulkanImageView imageView_;
  // an index into VulkanContext::textures_
  uint32_t textureId_ = 0;
};

} // namespace igl::vulkan
