/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanSampler.h>

namespace {
VkFilter samplerFilterToVkFilter(igl::SamplerFilter filter) {
  switch (filter) {
  case igl::SamplerFilter_Nearest:
    return VK_FILTER_NEAREST;
  case igl::SamplerFilter_Linear:
    return VK_FILTER_LINEAR;
  }
  IGL_ASSERT_MSG(false, "SamplerFilter value not handled: %d", (int)filter);
  return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode samplerMipMapToVkSamplerMipmapMode(igl::SamplerMip filter) {
  switch (filter) {
  case igl::SamplerMip_Disabled:
  case igl::SamplerMip_Nearest:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case igl::SamplerMip_Linear:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
  IGL_ASSERT_MSG(false, "SamplerMipMap value not handled: %d", (int)filter);
  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

static VkSamplerAddressMode samplerWrapModeToVkSamplerAddressMode(igl::SamplerWrap mode) {
  switch (mode) {
  case igl::SamplerWrap_Repeat:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case igl::SamplerWrap_Clamp:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case igl::SamplerWrap_MirrorRepeat:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  IGL_ASSERT_MSG(false, "SamplerWrapMode value not handled: %d", (int)mode);
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkSamplerCreateInfo samplerStateDescToVkSamplerCreateInfo(const igl::SamplerStateDesc& desc,
                                                          const VkPhysicalDeviceLimits& limits) {
  IGL_ASSERT_MSG(desc.mipLodMax >= desc.mipLodMin,
                 "mipLodMax (%d) must be greater than or equal to mipLodMin (%d)",
                 (int)desc.mipLodMax,
                 (int)desc.mipLodMin);

  VkSamplerCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = samplerFilterToVkFilter(desc.magFilter),
      .minFilter = samplerFilterToVkFilter(desc.minFilter),
      .mipmapMode = samplerMipMapToVkSamplerMipmapMode(desc.mipMap),
      .addressModeU = samplerWrapModeToVkSamplerAddressMode(desc.wrapU),
      .addressModeV = samplerWrapModeToVkSamplerAddressMode(desc.wrapV),
      .addressModeW = samplerWrapModeToVkSamplerAddressMode(desc.wrapW),
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0f,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = float(desc.mipLodMin),
      .maxLod = desc.mipMap == igl::SamplerMip_Disabled ? 0.0f : float(desc.mipLodMax),
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };

  if (desc.maxAnisotropic > 1) {
    const bool isAnisotropicFilteringSupported = limits.maxSamplerAnisotropy > 1;
    IGL_ASSERT_MSG(isAnisotropicFilteringSupported,
                   "Anisotropic filtering is not supported by the device.");
    ci.anisotropyEnable = isAnisotropicFilteringSupported ? VK_TRUE : VK_FALSE;

    if (limits.maxSamplerAnisotropy < desc.maxAnisotropic) {
      LLOGL(
          "Supplied sampler anisotropic value greater than max supported by the device, setting to "
          "%.0f",
          static_cast<double>(limits.maxSamplerAnisotropy));
    }
    ci.maxAnisotropy = std::min((float)limits.maxSamplerAnisotropy, (float)desc.maxAnisotropic);
  }

  if (desc.depthCompareEnabled) {
    ci.compareEnable = VK_TRUE;
    ci.compareOp = igl::vulkan::compareOpToVkCompareOp(desc.depthCompareOp);
  }

  return ci;
}
} // namespace

namespace igl {

namespace vulkan {

SamplerState::SamplerState(const igl::vulkan::Device& device) : device_(device) {}

SamplerState::~SamplerState() {
  // inform the context it should prune the samplers
  device_.getVulkanContext().awaitingDeletion_ = true;
}

Result SamplerState::create(const SamplerStateDesc& desc) {
  IGL_PROFILER_FUNCTION();

  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  Result result;
  sampler_ = ctx.createSampler(
      samplerStateDescToVkSamplerCreateInfo(desc, ctx.getVkPhysicalDeviceProperties().limits),
      &result,
      desc_.debugName);

  if (!IGL_VERIFY(result.isOk())) {
    return result;
  }

  return sampler_ ? Result() : Result(Result::Code::RuntimeError, "Cannot create VulkanSampler");
}

uint32_t SamplerState::getSamplerId() const {
  return sampler_ ? sampler_->samplerId_ : 0;
}

} // namespace vulkan

} // namespace igl
