/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanSampler.h>

#define IGL_VULKAN_DEBUG_SAMPLER_STATE 1

namespace {
VkFilter samplerMinMagFilterToVkFilter(igl::SamplerMinMagFilter filter) {
  switch (filter) {
  case igl::SamplerMinMagFilter::Nearest:
    return VK_FILTER_NEAREST;
  case igl::SamplerMinMagFilter::Linear:
    return VK_FILTER_LINEAR;
  }
  IGL_ASSERT_MSG(false, "SamplerMinMagFilter value not handled: %d", (int)filter);
  return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode samplerMipFilterToVkSamplerMipmapMode(igl::SamplerMipFilter filter) {
  switch (filter) {
  case igl::SamplerMipFilter::Disabled:
  case igl::SamplerMipFilter::Nearest:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case igl::SamplerMipFilter::Linear:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
  IGL_ASSERT_MSG(false, "SamplerMipFilter value not handled: %d", (int)filter);
  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode samplerAddressModeToVkSamplerAddressMode(igl::SamplerAddressMode mode) {
  switch (mode) {
  case igl::SamplerAddressMode::Repeat:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case igl::SamplerAddressMode::Clamp:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case igl::SamplerAddressMode::MirrorRepeat:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  IGL_ASSERT_MSG(false, "SamplerAddressMode value not handled: %d", (int)mode);
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkSamplerCreateInfo samplerStateDescToVkSamplerCreateInfo(const igl::SamplerStateDesc& desc,
                                                          const VkPhysicalDeviceLimits& limits) {
  IGL_ASSERT_MSG(desc.mipLodMax >= desc.mipLodMin,
                 "mipLodMax (%d) must be greater than or equal to mipLodMin (%d)",
                 (int)desc.mipLodMax,
                 (int)desc.mipLodMin);

  VkSamplerCreateInfo ci =
      ivkGetSamplerCreateInfo(samplerMinMagFilterToVkFilter(desc.minFilter),
                              samplerMinMagFilterToVkFilter(desc.magFilter),
                              samplerMipFilterToVkSamplerMipmapMode(desc.mipFilter),
                              samplerAddressModeToVkSamplerAddressMode(desc.addressModeU),
                              samplerAddressModeToVkSamplerAddressMode(desc.addressModeV),
                              samplerAddressModeToVkSamplerAddressMode(desc.addressModeW),
                              desc.mipLodMin,
                              desc.mipLodMax);

  if (desc.mipFilter == igl::SamplerMipFilter::Disabled) {
    ci.maxLod = 0.0f;
  }

  if (desc.maxAnisotropic > 1) {
    const bool isAnisotropicFilteringSupported = limits.maxSamplerAnisotropy > 1;
    IGL_ASSERT_MSG(isAnisotropicFilteringSupported,
                   "Anisotropic filtering is not supported by the device.");
    ci.anisotropyEnable = isAnisotropicFilteringSupported ? VK_TRUE : VK_FALSE;

#ifdef IGL_VULKAN_DEBUG_SAMPLER_STATE
    if (limits.maxSamplerAnisotropy < desc.maxAnisotropic) {
      IGL_LOG_INFO(
          "Supplied sampler anisotropic value greater than max supported by the device, setting to "
          "%.0f",
          static_cast<double>(limits.maxSamplerAnisotropy));
    }
#endif
    ci.maxAnisotropy = std::min((float)limits.maxSamplerAnisotropy, (float)desc.maxAnisotropic);
  }

  if (desc.depthCompareEnabled) {
    ci.compareEnable = VK_TRUE;
    ci.compareOp = igl::vulkan::compareFunctionToVkCompareOp(desc.depthCompareFunction);
  }

  return ci;
}
} // namespace

namespace igl::vulkan {

SamplerState::SamplerState(const igl::vulkan::Device& device) : device_(device) {}

Result SamplerState::create(const SamplerStateDesc& desc) {
  IGL_PROFILER_FUNCTION();

  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  Result result;
  sampler_ = ctx.createSampler(
      samplerStateDescToVkSamplerCreateInfo(desc, ctx.getVkPhysicalDeviceProperties().limits),
      textureFormatToVkFormat(desc.yuvFormat),
      &result,
      desc_.debugName.c_str());

  if (!IGL_VERIFY(result.isOk())) {
    return result;
  }

  return sampler_ ? Result()
                  : Result(Result::Code::InvalidOperation, "Cannot create VulkanSampler");
}

uint32_t SamplerState::getSamplerId() const {
  return sampler_ ? sampler_->samplerId_ : 0;
}

bool SamplerState::isYUV() const noexcept {
  return desc_.yuvFormat != igl::TextureFormat::Invalid;
}

} // namespace igl::vulkan
