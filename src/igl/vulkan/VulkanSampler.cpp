/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSampler.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

VulkanSampler::VulkanSampler(const VulkanContext& ctx) : ctx_(&ctx) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
}

VulkanSampler::VulkanSampler(VulkanSampler&& other) noexcept :
  ctx_(other.ctx_), samplerId_(other.samplerId_) {
  std::swap(vkSampler_, other.vkSampler_);
}

VulkanSampler& VulkanSampler::operator=(VulkanSampler&& other) noexcept {
  VulkanSampler tmp(std::move(other));
  std::swap(ctx_, tmp.ctx_);
  std::swap(vkSampler_, tmp.vkSampler_);
  std::swap(samplerId_, tmp.samplerId_);
  return *this;
}

} // namespace igl::vulkan
