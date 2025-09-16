/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/stb_image/TextureLoaderFactory.h>

namespace iglu::textureloader::stb::png {

/**
 * @brief ITextureLoaderFactory implementation for PNG files
 */

class TextureLoaderFactory final : public image::TextureLoaderFactory {
 public:
  TextureLoaderFactory() noexcept = default;

  [[nodiscard]] uint32_t minHeaderLength() const noexcept final;

 private:
  [[nodiscard]] bool isIdentifierValid(DataReader headerReader) const noexcept final;
};

} // namespace iglu::textureloader::stb::png
