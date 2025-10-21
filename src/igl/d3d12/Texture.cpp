/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Texture.h>

namespace igl::d3d12 {

std::shared_ptr<Texture> Texture::createFromResource(ID3D12Resource* resource,
                                                       TextureFormat format,
                                                       const TextureDesc& desc) {
  auto texture = std::make_shared<Texture>(format);
  texture->resource_ = resource;
  texture->format_ = format;
  texture->dimensions_ = Dimensions{desc.width, desc.height, desc.depth};
  texture->type_ = desc.type;
  texture->numLayers_ = desc.numLayers;
  texture->numMipLevels_ = desc.numMipLevels;
  texture->samples_ = desc.numSamples;
  texture->usage_ = desc.usage;

  // AddRef since we're storing a raw pointer from external source
  if (resource) {
    resource->AddRef();
  }

  return texture;
}

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

uint32_t Texture::getNumLayers() const {
  return static_cast<uint32_t>(numLayers_);
}

TextureType Texture::getType() const {
  return type_;
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return usage_;
}

uint32_t Texture::getSamples() const {
  return static_cast<uint32_t>(samples_);
}

uint32_t Texture::getNumMipLevels() const {
  return static_cast<uint32_t>(numMipLevels_);
}

uint64_t Texture::getTextureId() const {
  return reinterpret_cast<uint64_t>(resource_.Get());
}

TextureFormat Texture::getFormat() const {
  return format_;
}

bool Texture::isRequiredGenerateMipmap() const {
  return false;
}

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/,
                             const TextureRangeDesc* /*range*/) const {
  // TODO: Implement mipmap generation using compute shader or blit operations
}

void Texture::generateMipmap(ICommandBuffer& /*cmdBuffer*/,
                             const TextureRangeDesc* /*range*/) const {
  // TODO: Implement mipmap generation using compute shader or blit operations
}

} // namespace igl::d3d12
