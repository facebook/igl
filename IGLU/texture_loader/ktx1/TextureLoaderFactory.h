/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

namespace iglu::textureloader::ktx1 {

class TextureLoaderFactory final : public ITextureLoaderFactory {
 public:
  explicit TextureLoaderFactory() noexcept = default;

  [[nodiscard]] uint32_t headerLength() const noexcept final;

 private:
  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;
};

} // namespace iglu::textureloader::ktx1
