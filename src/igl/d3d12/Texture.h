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
  Texture(TextureFormat format) : format_(format) {}
  ~Texture() override = default;

  Result upload(const TextureRangeDesc& range,
                const void* data,
                size_t bytesPerRow = 0) const override;
  Result uploadCube(const TextureRangeDesc& range,
                   TextureCubeFace face,
                   const void* data,
                   size_t bytesPerRow = 0) const override;

  Dimensions getDimensions() const override;
  size_t getNumLayers() const override;
  TextureType getType() const override;
  TextureDesc::TextureUsage getUsage() const override;
  size_t getSamples() const override;
  size_t getNumMipLevels() const override;
  void generateMipmap(ICommandQueue& cmdQueue) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer) const override;
  uint64_t getTextureId() const override;
  TextureFormat getFormat() const override;
  bool isRequiredGenerateMipmap() const override;

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
