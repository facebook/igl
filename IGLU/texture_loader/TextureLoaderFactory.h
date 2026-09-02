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

  [[nodiscard]] uint32_t minHeaderLength() const noexcept final;
  [[nodiscard]] uint32_t maxHeaderLength() const noexcept final;

 protected:
  [[nodiscard]] bool canCreateInternal(DataReader headerReader,
                                       igl::Result* IGL_NULLABLE outResult) const noexcept final;

  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternal(
      DataReader reader,
      igl::TextureFormat preferredFormat,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;

 private:
  /// Every virtual is inline (trivial reads here, forwarders to non-virtual Impls in the .cpp
  /// for the rest), so this class has no key function and its vtable and typeinfo stay weak and
  /// are emitted per translation unit. The Impl bodies stay out of line because they use the
  /// profiler macros and igl::Result, which this header does not include.
  [[nodiscard]] bool canCreateInternalImpl(DataReader headerReader,
                                           igl::Result* IGL_NULLABLE outResult) const noexcept;
  [[nodiscard]] std::unique_ptr<ITextureLoader> tryCreateInternalImpl(
      DataReader reader,
      igl::TextureFormat preferredFormat,
      igl::Result* IGL_NULLABLE outResult) const noexcept;

  std::vector<std::unique_ptr<ITextureLoaderFactory>> factories_;
  uint32_t minHeaderLength_;
  uint32_t maxHeaderLength_;
};

inline uint32_t TextureLoaderFactory::minHeaderLength() const noexcept {
  return minHeaderLength_;
}

inline uint32_t TextureLoaderFactory::maxHeaderLength() const noexcept {
  return maxHeaderLength_;
}

inline bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                                    igl::Result* IGL_NULLABLE
                                                        outResult) const noexcept {
  return canCreateInternalImpl(headerReader, outResult);
}

inline std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat preferredFormat,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  return tryCreateInternalImpl(reader, preferredFormat, outResult);
}

} // namespace iglu::textureloader
