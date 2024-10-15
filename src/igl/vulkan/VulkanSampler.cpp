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

VulkanSampler::VulkanSampler(const VulkanContext& ctx,
                             const VkSamplerCreateInfo& ci,
                             const char* debugName) :
  ctx_(&ctx) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkDevice device = ctx_->getVkDevice();

  VK_ASSERT(ctx_->vf_.vkCreateSampler(device, &ci, nullptr, &vkSampler_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vkSampler_, debugName));
}

VulkanSampler::~VulkanSampler() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (ctx_ && vkSampler_) {
    ctx_->deferredTask(std::packaged_task<void()>(
        [vf = &ctx_->vf_, device = ctx_->getVkDevice(), sampler = vkSampler_]() {
          vf->vkDestroySampler(device, sampler, nullptr);
        }));
  }
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
