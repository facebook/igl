/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Texture.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Texture final : public ITexture {
 public:
  Texture() : ITexture(TextureFormat::Invalid), format_(TextureFormat::Invalid) {}
  explicit Texture(TextureFormat format) : ITexture(format), format_(format) {}
  ~Texture() override = default;

  // Factory method to create texture from existing D3D12 resource
  static std::shared_ptr<Texture> createFromResource(ID3D12Resource* resource,
                                                      TextureFormat format,
                                                      const TextureDesc& desc);

  // D3D12-specific upload methods (not part of ITexture interface)
  Result upload(const TextureRangeDesc& range,
                const void* data,
                size_t bytesPerRow = 0) const;
  Result uploadCube(const TextureRangeDesc& range,
                   TextureCubeFace face,
                   const void* data,
                   size_t bytesPerRow = 0) const;

  Dimensions getDimensions() const override;
  uint32_t getNumLayers() const override;
  TextureType getType() const override;
  TextureDesc::TextureUsage getUsage() const override;
  uint32_t getSamples() const override;
  uint32_t getNumMipLevels() const override;
  uint64_t getTextureId() const override;
  bool isRequiredGenerateMipmap() const override;

  void generateMipmap(ICommandQueue& cmdQueue,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;

  // D3D12-specific accessor (not part of ITexture interface)
  TextureFormat getFormat() const;

 private:
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  TextureFormat format_;
  Dimensions dimensions_{0, 0, 0};
  TextureType type_ = TextureType::TwoD;
  size_t numLayers_ = 1;
  size_t numMipLevels_ = 1;
  size_t samples_ = 1;
  TextureDesc::TextureUsage usage_ = 0;
};

} // namespace igl::d3d12
