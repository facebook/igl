/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/DataReader.h>
#include <igl/Texture.h>

namespace igl {
class ICapabilities;
class IDevice;
} // namespace igl

namespace iglu::textureloader {

/// Interface for getting CPU access to GPU texture data
class ITextureLoader {
 protected:
  explicit ITextureLoader(DataReader reader) noexcept;

 public:
  virtual ~ITextureLoader() = default;

  [[nodiscard]] const igl::TextureDesc& descriptor() const noexcept;

  [[nodiscard]] bool isSupported(const igl::ICapabilities& capabilities) const noexcept;
  [[nodiscard]] bool isSupported(const igl::ICapabilities& capabilities,
                                 igl::TextureDesc::TextureUsage usage) const noexcept;

  [[nodiscard]] std::shared_ptr<igl::ITexture> create(const igl::IDevice& device,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept;
  [[nodiscard]] std::shared_ptr<igl::ITexture> create(const igl::IDevice& device,
                                                      igl::TextureDesc::TextureUsage usage,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept;

  void upload(igl::ITexture& texture, igl::Result* IGL_NULLABLE outResult) const noexcept;

  [[nodiscard]] virtual bool shouldGenerateMipmaps() const noexcept {
    return desc_.numMipLevels > 1;
  }

 protected:
  [[nodiscard]] DataReader& reader() noexcept;
  [[nodiscard]] const DataReader& reader() const noexcept;

  [[nodiscard]] igl::TextureDesc& mutableDescriptor() noexcept;

  virtual void uploadInternal(igl::ITexture& texture,
                              igl::Result* IGL_NULLABLE outResult) const noexcept {
    auto result = texture.upload(texture.getFullMipRange(), reader_.data());
    igl::Result::setResult(outResult, std::move(result));
  }

 private:
  igl::TextureDesc desc_;
  DataReader reader_;
};

} // namespace iglu::textureloader
