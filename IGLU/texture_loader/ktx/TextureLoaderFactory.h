/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

struct ktxTexture;

namespace iglu::textureloader::ktx {

/**
 * @brief ITextureLoaderFactory base class for loading KTX v1 and v2 textures
 */
class TextureLoaderFactory : public ITextureLoaderFactory {
 protected:
  TextureLoaderFactory() noexcept = default;
  [[nodiscard]] virtual igl::TextureRangeDesc textureRange(DataReader reader) const noexcept = 0;

  [[nodiscard]] virtual bool validate(DataReader reader,
                                      const igl::TextureRangeDesc& range,
                                      igl::Result* IGL_NULLABLE outResult) const noexcept = 0;

  [[nodiscard]] virtual igl::TextureFormat textureFormat(
      const ktxTexture* IGL_NONNULL texture) const noexcept = 0;

 private:
  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::TextureFormat preferredFormat, // Ignored for KTX textures
      igl::Result* IGL_NULLABLE outResult) const noexcept final;
};

} // namespace iglu::textureloader::ktx
