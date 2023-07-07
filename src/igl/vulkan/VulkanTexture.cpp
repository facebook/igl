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

namespace igl {

namespace vulkan {

VulkanTexture::VulkanTexture(const VulkanContext& ctx,
                             std::shared_ptr<VulkanImage> image,
                             std::shared_ptr<VulkanImageView> imageView) :
  ctx_(ctx), image_(std::move(image)), imageView_(std::move(imageView)) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT(image_);
  IGL_ASSERT(imageView_);
}

VulkanTexture::~VulkanTexture() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  // inform the context it should prune the textures
  ctx_.awaitingDeletion_ = true;
}

} // namespace vulkan

} // namespace igl
