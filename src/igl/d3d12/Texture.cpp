/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Texture.h>

namespace igl::d3d12 {

Result Texture::upload(const TextureRangeDesc& /*range*/,
                      const void* /*data*/,
                      size_t /*bytesPerRow*/) const {
  return Result(Result::Code::Unimplemented, "D3D12 Texture::upload not yet implemented");
}

Result Texture::uploadCube(const TextureRangeDesc& /*range*/,
                          TextureCubeFace /*face*/,
                          const void* /*data*/,
                          size_t /*bytesPerRow*/) const {
  return Result(Result::Code::Unimplemented, "D3D12 Texture::uploadCube not yet implemented");
}

Dimensions Texture::getDimensions() const {
  return dimensions_;
}

size_t Texture::getNumLayers() const {
  return numLayers_;
}

TextureType Texture::getType() const {
  return type_;
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return usage_;
}

size_t Texture::getSamples() const {
  return samples_;
}

size_t Texture::getNumMipLevels() const {
  return numMipLevels_;
}

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/) const {}

void Texture::generateMipmap(ICommandBuffer& /*cmdBuffer*/) const {}

uint64_t Texture::getTextureId() const {
  return reinterpret_cast<uint64_t>(resource_.Get());
}

TextureFormat Texture::getFormat() const {
  return format_;
}

bool Texture::isRequiredGenerateMipmap() const {
  return false;
}

} // namespace igl::d3d12
