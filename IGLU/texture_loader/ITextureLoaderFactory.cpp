/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ITextureLoaderFactory.h>

namespace iglu::textureloader {

bool ITextureLoaderFactory::isHeaderValid(const uint8_t* IGL_NONNULL data,
                                          uint32_t length,
                                          igl::Result* IGL_NULLABLE outResult) const noexcept {
  auto maybeReader = DataReader::tryCreate(data, length, outResult);
  if (!maybeReader.has_value()) {
    return false;
  }

  return isHeaderValid(*maybeReader, outResult);
}

bool ITextureLoaderFactory::isHeaderValid(DataReader reader,
                                          igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (reader.data() == nullptr) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Reader's data is nullptr.");
    return false;
  }
  if (reader.length() < headerLength()) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentOutOfRange, "Not enough data for header.");
    return false;
  }

  return isHeaderValidInternal(reader, outResult);
}

std::unique_ptr<ITextureLoader> ITextureLoaderFactory::tryCreate(const uint8_t* IGL_NONNULL data,
                                                                 uint32_t length,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept {
  auto maybeReader = DataReader::tryCreate(data, length, outResult);
  if (!maybeReader.has_value()) {
    return nullptr;
  }

  return tryCreate(*maybeReader, outResult);
}

std::unique_ptr<ITextureLoader> ITextureLoaderFactory::tryCreate(DataReader reader,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept {
  if (!isHeaderValid(reader, outResult)) {
    return nullptr;
  }

  return tryCreateInternal(reader, outResult);
}

} // namespace iglu::textureloader
