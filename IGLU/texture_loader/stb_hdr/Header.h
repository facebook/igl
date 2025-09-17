/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>

namespace iglu::textureloader::stb::hdr {

using Tag = std::array<uint8_t, 11>;

struct Header {
  Tag tag;

  [[nodiscard]] bool tagIsValid() const noexcept;
};
static_assert(sizeof(Header) == 11);

constexpr uint32_t kHeaderLength = static_cast<uint32_t>(sizeof(Header));

} // namespace iglu::textureloader::stb::hdr
