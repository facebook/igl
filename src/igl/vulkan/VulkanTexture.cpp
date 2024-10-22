/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanTexture.h"

namespace igl::vulkan {

VulkanTexture::VulkanTexture(VulkanImage&& image, VulkanImageView&& imageView) :
  image_(std::move(image)), imageView_(std::move(imageView)) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_DEBUG_ASSERT(image_.valid());
  IGL_DEBUG_ASSERT(imageView_.valid());
}

} // namespace igl::vulkan
