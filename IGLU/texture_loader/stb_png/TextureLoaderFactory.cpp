/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_png/TextureLoaderFactory.h>

#include <IGLU/texture_loader/stb_png/Header.h>

namespace iglu::textureloader::stb::png {

uint32_t TextureLoaderFactory::headerLength() const noexcept {
  return kHeaderLength;
}

bool TextureLoaderFactory::isIdentifierValid(DataReader headerReader) const noexcept {
  const Header* header = headerReader.as<Header>();
  return header->tagIsValid();
}

} // namespace iglu::textureloader::stb::png
