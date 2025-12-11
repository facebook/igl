/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/PlatformDevice.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

PlatformDevice::PlatformDevice(Device& device) : device_(device) {}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDepth(uint32_t width,
                                                                       uint32_t height,
                                                                       Result* outResult) {
  auto& ctx = device_.getD3D12Context();

  // Create depth texture with D3D12
  TextureDesc depthDesc = TextureDesc::new2D(TextureFormat::Z_UNorm32,
                                            width,
                                            height,
                                            TextureDesc::TextureUsageBits::Attachment,
                                            "Swapchain Depth Texture");

  // Allocate new depth texture if null or mismatches in size
  if (!nativeDepthTexture_ || width != nativeDepthTexture_->getDimensions().width ||
      height != nativeDepthTexture_->getDimensions().height) {
    nativeDepthTexture_ = device_.createTexture(depthDesc, outResult);
  }

  Result::setResult(outResult, Result::Code::Ok);
  return nativeDepthTexture_;
}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(Result* outResult) {
  IGL_D3D12_LOG_VERBOSE("PlatformDevice::createTextureFromNativeDrawable() called\n");
  auto& ctx = device_.getD3D12Context();

  // Get current back buffer from swapchain
  uint32_t backBufferIndex = ctx.getCurrentBackBufferIndex();
  ID3D12Resource* backBuffer = ctx.getCurrentBackBuffer();

  IGL_D3D12_LOG_VERBOSE("  backBufferIndex=%u, backBuffer=%p\n", backBufferIndex, backBuffer);

  if (!backBuffer) {
    IGL_LOG_ERROR("  No back buffer available!\n");
    Result::setResult(outResult, Result::Code::RuntimeError, "No back buffer available");
    return nullptr;
  }

  // Get back buffer description
  D3D12_RESOURCE_DESC desc = backBuffer->GetDesc();
  const auto width = static_cast<uint32_t>(desc.Width);
  const auto height = static_cast<uint32_t>(desc.Height);

  // Determine texture format based on DXGI format
  // IMPORTANT: Use dxgiFormatToTextureFormat() to get the CORRECT IGL format
  // from the actual D3D12 resource format. Do NOT hardcode RGBA_SRGB!
  igl::TextureFormat iglFormat = dxgiFormatToTextureFormat(desc.Format);
  if (iglFormat == igl::TextureFormat::Invalid) {
    IGL_LOG_ERROR("  Unsupported DXGI format: %d\n", desc.Format);
    Result::setResult(outResult, Result::Code::RuntimeError, "Unsupported swapchain DXGI format");
    return nullptr;
  }

  // Ensure we have enough cached textures for swapchain images
  while (nativeDrawableTextures_.size() <= backBufferIndex) {
    nativeDrawableTextures_.push_back(nullptr);
  }

  // Allocate new drawable texture if null or mismatches
  if (!nativeDrawableTextures_[backBufferIndex] ||
      width != nativeDrawableTextures_[backBufferIndex]->getDimensions().width ||
      height != nativeDrawableTextures_[backBufferIndex]->getDimensions().height) {

    TextureDesc textureDesc;
    textureDesc.type = TextureType::TwoD;
    textureDesc.format = iglFormat;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.depth = 1;
    textureDesc.numLayers = 1;
    textureDesc.numSamples = 1;
    textureDesc.numMipLevels = 1;
    textureDesc.usage = TextureDesc::TextureUsageBits::Attachment;
    textureDesc.debugName = "Swapchain Back Buffer";

    nativeDrawableTextures_[backBufferIndex] = Texture::createFromResource(
        backBuffer,
        iglFormat,
        textureDesc,
        ctx.getDevice(),
        ctx.getCommandQueue(),
        D3D12_RESOURCE_STATE_PRESENT);

    if (!nativeDrawableTextures_[backBufferIndex]) {
      Result::setResult(outResult, Result::Code::RuntimeError,
                       "Failed to create texture from back buffer");
      return nullptr;
    }
  }

  Result::setResult(outResult, Result::Code::Ok);
  return nativeDrawableTextures_[backBufferIndex];
}

} // namespace igl::d3d12
