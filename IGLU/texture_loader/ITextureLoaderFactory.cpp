/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

namespace iglu::textureloader {

bool ITextureLoaderFactory::canCreate(const uint8_t* IGL_NONNULL headerData,
                                      uint32_t headerLength,
                                      igl::Result* IGL_NULLABLE outResult) const noexcept {
  auto maybeReader = DataReader::tryCreate(headerData, headerLength, outResult);
  if (!maybeReader.has_value()) {
    return false;
  }

  return canCreate(*maybeReader, outResult);
}

bool ITextureLoaderFactory::canCreate(DataReader headerReader,
                                      igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (headerReader.data() == nullptr) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Reader's data is nullptr.");
    return false;
  }
  if (headerReader.length() < headerLength()) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentOutOfRange, "Not enough data for header.");
    return false;
  }

  return canCreateInternal(headerReader, outResult);
}

std::unique_ptr<ITextureLoader> ITextureLoaderFactory::tryCreate(const uint8_t* IGL_NONNULL data,
                                                                 uint32_t length,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept {
  return tryCreate(data, length, igl::TextureFormat::Invalid, outResult);
}

std::unique_ptr<ITextureLoader> ITextureLoaderFactory::tryCreate(const uint8_t* IGL_NONNULL data,
                                                                 uint32_t length,
                                                                 igl::TextureFormat preferredFormat,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept {
  auto maybeReader = DataReader::tryCreate(data, length, outResult);
  if (!maybeReader.has_value()) {
    return nullptr;
  }

  return tryCreate(*maybeReader, preferredFormat, outResult);
}

std::unique_ptr<ITextureLoader> ITextureLoaderFactory::tryCreate(DataReader reader,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept {
  return tryCreate(reader, igl::TextureFormat::Invalid, outResult);
}

std::unique_ptr<ITextureLoader> ITextureLoaderFactory::tryCreate(DataReader reader,
                                                                 igl::TextureFormat preferredFormat,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept {
  if (!canCreate(reader, outResult)) {
    return nullptr;
  }

  return tryCreateInternal(reader, preferredFormat, outResult);
}

} // namespace iglu::textureloader
