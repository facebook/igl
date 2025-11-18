/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_png/TextureLoaderFactory.h>

#include <IGLU/texture_loader/stb_png/Header.h>

namespace iglu::textureloader::stb::png {

uint32_t TextureLoaderFactory::minHeaderLength() const noexcept {
  // Require enough bytes for the full minimal PNG structure used by tests:
  // - 8-byte file signature
  // - IHDR chunk header + data + CRC
  // - IDAT chunk header + CRC (empty data)
  //
  // This ensures truncated buffers that still contain a valid PNG signature
  // are rejected early, while minimally valid headers succeed.
  constexpr uint32_t kMinimalPngHeaderLength = 45u;
  return kMinimalPngHeaderLength;
}

bool TextureLoaderFactory::isIdentifierValid(DataReader headerReader) const noexcept {
  const Header* header = headerReader.as<Header>();
  return header->tagIsValid();
}

} // namespace iglu::textureloader::stb::png
