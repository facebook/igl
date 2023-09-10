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

namespace iglu::textureloader::ktx2 {

using Tag = std::array<uint8_t, 12>;

struct Header {
  Tag tag;
  uint32_t vkFormat;
  uint32_t typeSize;
  uint32_t pixelWidth;
  uint32_t pixelHeight;
  uint32_t pixelDepth;
  uint32_t layerCount;
  uint32_t faceCount;
  uint32_t levelCount;
  uint32_t supercompressionScheme;

  uint32_t dfdByteOffset;
  uint32_t dfdByteLength;
  uint32_t kvdByteOffset;
  uint32_t kvdByteLength;
  uint64_t sgdByteOffset;
  uint64_t sgdByteLength;

  [[nodiscard]] bool tagIsValid() const noexcept;
  [[nodiscard]] igl::TextureFormatProperties formatProperties() const noexcept;
};
static_assert(sizeof(Header) == 13 * sizeof(uint32_t) + 2 * sizeof(uint64_t) + 12);

constexpr uint32_t kHeaderLength = static_cast<uint32_t>(sizeof(Header));

} // namespace iglu::textureloader::ktx2
