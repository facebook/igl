/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ktx/TextureLoaderFactory.h>

namespace iglu::textureloader::ktx1 {

/**
 * @brief ITextureLoaderFactory implementation for KTX v1 texture containers
 * @note Texture container format specifications:
 *   https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html
 */
class TextureLoaderFactory final : public ktx::TextureLoaderFactory {
 public:
  explicit TextureLoaderFactory() noexcept = default;

  [[nodiscard]] uint32_t minHeaderLength() const noexcept final;

 private:
  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] igl::TextureRangeDesc textureRange(DataReader reader) const noexcept final;

  [[nodiscard]] bool validate(DataReader reader,
                              const igl::TextureRangeDesc& range,
                              igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] igl::TextureFormat textureFormat(
      const ktxTexture* IGL_NONNULL texture) const noexcept final;
};

} // namespace iglu::textureloader::ktx1
