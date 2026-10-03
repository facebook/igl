/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/SamplerState.h>

#include <algorithm>
#include <atomic>
#include <optional>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

uint64_t nextSamplerId() {
  static std::atomic<uint64_t> counter = 0;
  return ++counter;
}

} // namespace

std::shared_ptr<SamplerState> SamplerState::create(const WebGPUContext& ctx,
                                                   const SamplerStateDesc& desc,
                                                   Result* IGL_NULLABLE outResult) {
  if (desc.yuvFormat != TextureFormat::Invalid) {
    Result::setResult(outResult, Result::Code::Unsupported, "YUV samplers are not supported");
    return nullptr;
  }
  const std::optional<WGPUAddressMode> addressU =
      samplerAddressModeToWGPUAddressMode(desc.addressModeU);
  const std::optional<WGPUAddressMode> addressV =
      samplerAddressModeToWGPUAddressMode(desc.addressModeV);
  const std::optional<WGPUAddressMode> addressW =
      samplerAddressModeToWGPUAddressMode(desc.addressModeW);
  if (!addressU || !addressV || !addressW) {
    Result::setResult(
        outResult, Result::Code::Unsupported, "WebGPU has no clamp-to-border address mode");
    return nullptr;
  }

  WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
  samplerDesc.label = toWGPUStringView(desc.debugName);
  samplerDesc.addressModeU = *addressU;
  samplerDesc.addressModeV = *addressV;
  samplerDesc.addressModeW = *addressW;
  samplerDesc.magFilter = samplerMinMagFilterToWGPUFilterMode(desc.magFilter);
  samplerDesc.minFilter = samplerMinMagFilterToWGPUFilterMode(desc.minFilter);
  samplerDesc.mipmapFilter = samplerMipFilterToWGPUMipmapFilterMode(desc.mipFilter);
  // A disabled mip filter samples only the base level.
  const bool mipsDisabled = desc.mipFilter == SamplerMipFilter::Disabled;
  samplerDesc.lodMinClamp = mipsDisabled ? 0.0f : static_cast<float>(desc.mipLodMin);
  samplerDesc.lodMaxClamp =
      mipsDisabled ? 0.0f : static_cast<float>(std::max(desc.mipLodMin, desc.mipLodMax));
  samplerDesc.compare = desc.depthCompareEnabled
                            ? compareFunctionToWGPUCompareFunction(desc.depthCompareFunction)
                            : WGPUCompareFunction_Undefined;
  samplerDesc.maxAnisotropy = std::clamp<uint16_t>(desc.maxAnisotropic, 1, 16);
  const bool allLinear = desc.minFilter == SamplerMinMagFilter::Linear &&
                         desc.magFilter == SamplerMinMagFilter::Linear &&
                         desc.mipFilter == SamplerMipFilter::Linear;
  if (samplerDesc.maxAnisotropy > 1 && !allLinear) {
    // WebGPU requires linear min, mag and mip filters for anisotropic sampling.
    IGL_LOG_INFO_ONCE("WebGPU ignores maxAnisotropic unless all sampler filters are linear\n");
    samplerDesc.maxAnisotropy = 1;
  }

  auto sampler = std::shared_ptr<SamplerState>(new SamplerState(ctx, samplerDesc));
  if (!sampler->sampler_) {
    Result::setResult(outResult, Result::Code::RuntimeError, "wgpuDeviceCreateSampler() failed");
    return nullptr;
  }
  Result::setOk(outResult);
  return sampler;
}

SamplerState::SamplerState(const WebGPUContext& ctx, const WGPUSamplerDescriptor& desc) :
  ctx_(ctx),
  desc_(desc),
  sampler_(wgpuDeviceCreateSampler(ctx.getDevice(), &desc)),
  isFiltering_(desc.magFilter == WGPUFilterMode_Linear || desc.minFilter == WGPUFilterMode_Linear ||
               desc.mipmapFilter == WGPUMipmapFilterMode_Linear),
  isComparison_(desc.compare != WGPUCompareFunction_Undefined),
  samplerId_(nextSamplerId()) {
  // The label points into the caller's descriptor.
  desc_.label = toWGPUStringView(nullptr);
}

WGPUSampler IGL_NULLABLE SamplerState::getNonFilteringSampler() const {
  if (!isFiltering_) {
    return sampler_.get();
  }
  if (!nonFilteringSampler_) {
    WGPUSamplerDescriptor desc = desc_;
    desc.magFilter = WGPUFilterMode_Nearest;
    desc.minFilter = WGPUFilterMode_Nearest;
    desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    desc.maxAnisotropy = 1;
    nonFilteringSampler_.reset(wgpuDeviceCreateSampler(ctx_.getDevice(), &desc));
  }
  return nonFilteringSampler_.get();
}

} // namespace igl::webgpu
