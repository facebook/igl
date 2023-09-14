/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

namespace iglu::textureloader::stb::image {

class TextureLoaderFactory : public ITextureLoaderFactory {
 public:
  explicit TextureLoaderFactory() noexcept = default;

 protected:
  [[nodiscard]] virtual bool isFloatFormat() const noexcept = 0;
  [[nodiscard]] virtual igl::TextureFormat format() const noexcept = 0;

 private:
  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;
};

} // namespace iglu::textureloader::stb::image
