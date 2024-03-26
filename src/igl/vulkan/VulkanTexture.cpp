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

namespace igl {

namespace vulkan {

VulkanTexture::VulkanTexture(const VulkanContext& ctx,
                             std::shared_ptr<VulkanImage> image,
                             std::unique_ptr<VulkanImageView> imageView) :
  ctx_(ctx), image_(std::move(image)), imageView_(std::move(imageView)) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT(image_);
  IGL_ASSERT(imageView_);
}

} // namespace vulkan

} // namespace igl
