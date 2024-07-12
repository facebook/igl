/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/texture_loader/ITextureLoaderFactory.h>
#include <vector>

namespace iglu::textureloader {

/// Factory for creating ITextureLoader instances for supported formats.
class TextureLoaderFactory : public ITextureLoaderFactory {
  using Super = ITextureLoaderFactory;

 public:
  explicit TextureLoaderFactory(std::vector<std::unique_ptr<ITextureLoaderFactory>>&& factories);
  ~TextureLoaderFactory() override = default;

  [[nodiscard]] uint32_t headerLength() const noexcept final;

 protected:
  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::TextureFormat preferredFormat,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;

 private:
  std::vector<std::unique_ptr<ITextureLoaderFactory>> factories_;
  uint32_t headerLength_;
};

} // namespace iglu::textureloader
