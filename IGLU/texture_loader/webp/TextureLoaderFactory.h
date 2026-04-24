/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

namespace iglu::textureloader::webp {

/**
 * @brief ITextureLoaderFactory implementation for WebP files.
 *
 * Decodes WebP images to RGBA using libwebp. Supports both lossy and lossless
 * WebP, with and without alpha channel.
 */
class TextureLoaderFactory final : public ITextureLoaderFactory {
 public:
  TextureLoaderFactory() noexcept = default;

  [[nodiscard]] uint32_t minHeaderLength() const noexcept final;

 private:
  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::TextureFormat preferredFormat,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;
};

} // namespace iglu::textureloader::webp
