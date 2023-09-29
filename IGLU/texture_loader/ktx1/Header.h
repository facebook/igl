/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>
#include <igl/Texture.h>

namespace iglu::textureloader::ktx1 {

using Tag = std::array<uint8_t, 12>;

struct Header {
  Tag tag;
  uint32_t endianness; // always little Endian
  uint32_t glType; // For compressed textures, this should always be 0
  uint32_t glTypeSize; // For compressed textures, this should always be 1
  uint32_t glFormat;
  uint32_t glInternalFormat;
  uint32_t glBaseInternalFormat;
  uint32_t pixelWidth;
  uint32_t pixelHeight;
  uint32_t pixelDepth;
  uint32_t numberOfArrayElements; // Always 0 for non-array textures
  uint32_t numberOfFaces; // Always 1 for non cubemap textures
  uint32_t numberOfMipmapLevels;
  uint32_t bytesOfKeyValueData; // 0 - extra key-value isn't needed at the moment

  [[nodiscard]] bool tagIsValid() const noexcept;
};
static_assert(sizeof(Header) == 13 * sizeof(uint32_t) + 12);

constexpr uint32_t kHeaderLength = static_cast<uint32_t>(sizeof(Header));

} // namespace iglu::textureloader::ktx1
