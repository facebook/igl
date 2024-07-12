/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

namespace iglu::textureloader::stb::image {

/**
 * @brief ITextureLoaderFactory base class for loading textures with STB Image
 */
class TextureLoaderFactory : public ITextureLoaderFactory {
 protected:
  explicit TextureLoaderFactory(bool isFloatFormat = false) noexcept;

  [[nodiscard]] virtual bool isIdentifierValid(DataReader headerReader) const noexcept = 0;

 private:
  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::TextureFormat preferredFormat,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;

  bool isFloatFormat_;
};

} // namespace iglu::textureloader::stb::image
