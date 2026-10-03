/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/PlatformDevice.h>

#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/Readback.h>
#include <igl/webgpu/Texture.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

WebGPUContext& PlatformDevice::getContext() const noexcept {
  return device_.getContext();
}

WGPUInstance IGL_NULLABLE PlatformDevice::getWGPUInstance() const noexcept {
  return getContext().getInstance();
}

WGPUAdapter IGL_NULLABLE PlatformDevice::getWGPUAdapter() const noexcept {
  return getContext().getAdapter();
}

WGPUDevice IGL_NULLABLE PlatformDevice::getWGPUDevice() const noexcept {
  return getContext().getDevice();
}

WGPUQueue IGL_NULLABLE PlatformDevice::getWGPUQueue() const noexcept {
  return getContext().getQueue();
}

bool PlatformDevice::hasWGPUFeature(WGPUFeatureName feature) const {
  return device_.getDeviceFeatureSet().hasWGPUFeature(feature);
}

void PlatformDevice::setSuspensionAllowed(bool allowed) noexcept {
  getContext().setSuspensionAllowed(allowed);
}

bool PlatformDevice::isSuspensionAllowed() const noexcept {
  return getContext().isSuspensionAllowed();
}

std::vector<Result> PlatformDevice::takeErrors() {
  return getContext().takeErrors();
}

Result PlatformDevice::readPixelsAsync(const ITexture& texture,
                                       const TextureRangeDesc& range,
                                       AsyncTextureReadback& readback,
                                       bool flipVertically) const {
  const auto& webgpuTexture = static_cast<const Texture&>(texture);
  const TextureFormatProperties& props = texture.getProperties();
  const WGPUTextureAspect aspect = !props.isDepthOrStencil() ? WGPUTextureAspect_All
                                   : props.hasDepth()        ? WGPUTextureAspect_DepthOnly
                                                             : WGPUTextureAspect_StencilOnly;
  const uint32_t bytesPerTexel = getCopyBytesPerTexel(webgpuTexture.getWGPUFormat(), aspect);
  if (bytesPerTexel == 0) {
    return Result(Result::Code::Unsupported, "This texture format cannot be read back");
  }
  if (range.numMipLevels != 1 || range.numLayers != 1 || range.numFaces != 1 || range.depth != 1) {
    return Result(Result::Code::Unsupported, "Readbacks cover one mip level and layer");
  }
  // Same rules as Texture::getBytes(); the copy would otherwise fail validation, which latched
  // error modes only report later.
  if (texture.getSamples() != 1) {
    return Result(Result::Code::Unsupported, "Multisampled textures cannot be read back");
  }
  if (range.mipLevel >= texture.getNumMipLevels()) {
    return Result(Result::Code::ArgumentOutOfRange, "The readback is outside the mip levels");
  }
  const TextureRangeDesc level = texture.getFullRange(range.mipLevel);
  const bool is3D = texture.getType() == TextureType::ThreeD;
  const bool isCube = texture.getType() == TextureType::Cube;
  const bool sliceOutside = is3D ? range.z >= level.depth
                                 : range.z != 0 || range.layer >= texture.getNumLayers() ||
                                       range.face >= (isCube ? 6u : 1u);
  if (range.x > level.width || range.width > level.width - range.x || range.y > level.height ||
      range.height > level.height - range.y || sliceOutside) {
    return Result(Result::Code::ArgumentOutOfRange, "The readback is outside the mip level");
  }
  return readback.begin(
      getContext(),
      {
          .texture = webgpuTexture.getWGPUTexture(),
          .aspect = aspect,
          .mipLevel = webgpuTexture.getBaseMipLevel() + range.mipLevel,
          .layer = is3D ? range.z : webgpuTexture.getWGPULayer(range.layer, range.face),
          .x = range.x,
          .y = range.y,
          .width = range.width,
          .height = range.height,
          .bytesPerTexel = bytesPerTexel,
          .flipVertically = flipVertically,
      });
}

Result PlatformDevice::mapBufferAsync(const IBuffer& buffer,
                                      size_t offset,
                                      size_t size,
                                      AsyncBufferReadback& readback) const {
  if (offset > buffer.getSizeInBytes() || size > buffer.getSizeInBytes() - offset) {
    return Result(Result::Code::ArgumentOutOfRange, "The range exceeds the buffer");
  }
  return readback.begin(
      getContext(), static_cast<const Buffer&>(buffer).getWGPUBuffer(), offset, size);
}

} // namespace igl::webgpu
