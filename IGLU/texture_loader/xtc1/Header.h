/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>

namespace iglu::textureloader::xtc1 {

using Tag = std::array<uint8_t, 4>;

struct Header {
  static constexpr uint32_t kMaxMips = 12;
  static constexpr uint32_t kVersion = 0x00010002;

  Tag magicTag{0x49, 0x56, 0x41, 0x4e};
  uint32_t version{kVersion};
  uint32_t width{0};
  uint32_t height{0};
  union {
    struct {
      uint32_t numChannels : 3;
      uint32_t lossless : 1;
      uint32_t impasto : 1;
      uint32_t numMips : 4;
    };
    uint32_t flags{0};
  };
  uint32_t mipSizes[kMaxMips]{};
  uint32_t padding{0};

  [[nodiscard]] bool tagIsValid() const noexcept;
};

constexpr uint32_t kHeaderLength = static_cast<uint32_t>(sizeof(Header));

} // namespace iglu::textureloader::xtc1
