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
                             VkDevice device,
                             const VkSamplerCreateInfo& ci,
                             VkFormat yuvVkFormat,
                             const char* debugName) :
  ctx_(ctx), device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkSamplerCreateInfo cInfo = ci;
  VkSamplerYcbcrConversionInfo conversionInfo{};

  if (yuvVkFormat != VK_FORMAT_UNDEFINED) {
    conversionInfo = ctx.getOrCreateYcbcrConversionInfo(yuvVkFormat);
    cInfo.pNext = &conversionInfo;
    // must be CLAMP_TO_EDGE
    // https://vulkan.lunarg.com/doc/view/1.3.268.0/windows/1.3-extensions/vkspec.html#VUID-VkSamplerCreateInfo-addressModeU-01646
    cInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    cInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    cInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    cInfo.anisotropyEnable = VK_FALSE;
    cInfo.unnormalizedCoordinates = VK_FALSE;
  }

  VK_ASSERT(ctx_.vf_.vkCreateSampler(device_, &cInfo, nullptr, &vkSampler_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_.vf_, device_, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vkSampler_, debugName));

  setDebugName(debugName);
}

VulkanSampler::~VulkanSampler() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  ctx_.deferredTask(
      std::packaged_task<void()>([vf = &ctx_.vf_, device = device_, sampler = vkSampler_]() {
        vf->vkDestroySampler(device, sampler, nullptr);
      }));
}

void VulkanSampler::setDebugName(const std::string& debugName) noexcept {
#if defined(IGL_DEBUG)
  debugName_ = debugName;
#else
  (void)debugName;
#endif
}

} // namespace igl::vulkan
