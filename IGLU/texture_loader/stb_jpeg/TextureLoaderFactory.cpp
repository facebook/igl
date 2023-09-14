/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_jpeg/TextureLoaderFactory.h>

#include <IGLU/texture_loader/stb_jpeg/Header.h>
#include <igl/IGLSafeC.h>
#include <numeric>
#include <vector>

namespace iglu::textureloader::stb::jpeg {

uint32_t TextureLoaderFactory::headerLength() const noexcept {
  return kHeaderLength;
}

bool TextureLoaderFactory::isFloatFormat() const noexcept {
  return false;
}

igl::TextureFormat TextureLoaderFactory::format() const noexcept {
  return igl::TextureFormat::RGBA_SRGB;
}

bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (headerReader.data() == nullptr) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Reader's data is nullptr.");
    return false;
  }
  if (headerReader.length() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentOutOfRange, "Not enough data for header.");
    return false;
  }

  const Header* header = headerReader.as<Header>();
  if (!header->tagIsValid()) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Incorrect identifier.");
    return false;
  }

  return true;
}

} // namespace iglu::textureloader::stb::jpeg
