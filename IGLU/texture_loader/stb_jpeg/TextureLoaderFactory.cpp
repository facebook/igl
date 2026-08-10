/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_jpeg/TextureLoaderFactory.h>

#include <IGLU/texture_loader/stb_jpeg/Header.h>
#include <igl/Macros.h>

namespace iglu::textureloader::stb::jpeg {

uint32_t TextureLoaderFactory::minHeaderLength() const noexcept {
  return kHeaderLength;
}

bool TextureLoaderFactory::isIdentifierValid(DataReader headerReader) const noexcept {
  IGL_PROFILER_FUNCTION();
  const Header* header = headerReader.as<Header>();
  return header->tagIsValid();
}

} // namespace iglu::textureloader::stb::jpeg
