/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoader.h>

#include <vector>

namespace iglu::textureloader::ktx2 {

struct Header;

/**
 * @brief ITextureLoader implementation for KTX v2 texture containers
 * @note Texture container format specifications:
 *   https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html
 */
class TextureLoader : public ITextureLoader {
  using Super = ITextureLoader;

 public:
  [[nodiscard]] static bool canCreate(DataReader headerReader,
                                      igl::Result* IGL_NULLABLE outResult) noexcept;

  [[nodiscard]] static std::unique_ptr<TextureLoader> tryCreate(DataReader reader,
                                                                igl::Result* IGL_NULLABLE
                                                                    outResult) noexcept;

  [[nodiscard]] bool shouldGenerateMipmaps() const noexcept final;

 private:
  TextureLoader(DataReader reader,
                const Header& header,
                igl::TextureFormat format,
                std::vector<const uint8_t*> mipData) noexcept;

  void uploadInternal(igl::ITexture& texture,
                      igl::Result* IGL_NULLABLE outResult) const noexcept final;

  std::vector<const uint8_t*> mipData_;
  bool shouldGenerateMipmaps_ = false;
};

} // namespace iglu::textureloader::ktx2
