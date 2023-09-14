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
  explicit TextureLoaderFactory() noexcept = default;

  [[nodiscard]] uint32_t headerLength() const noexcept final;

 private:
  [[nodiscard]] bool isFloatFormat() const noexcept final;
  [[nodiscard]] igl::TextureFormat format() const noexcept final;

  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;
};

} // namespace iglu::textureloader::stb::png
