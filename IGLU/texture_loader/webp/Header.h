/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>

namespace iglu::textureloader::webp {

// WebP files start with "RIFF" (4 bytes) + file size (4 bytes) + "WEBP" (4 bytes)
struct Header {
  std::array<uint8_t, 4> riff;
  uint32_t fileSize;
  std::array<uint8_t, 4> webp;

  [[nodiscard]] bool tagIsValid() const noexcept;
};
static_assert(sizeof(Header) == 12);

constexpr uint32_t kHeaderLength = static_cast<uint32_t>(sizeof(Header));

} // namespace iglu::textureloader::webp
