/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/webp/Header.h>

#include <cstring>

namespace iglu::textureloader::webp {
namespace {
constexpr std::array<uint8_t, 4> kRiffTag = {{'R', 'I', 'F', 'F'}};
constexpr std::array<uint8_t, 4> kWebpTag = {{'W', 'E', 'B', 'P'}};
} // namespace

bool Header::tagIsValid() const noexcept {
  return std::memcmp(riff.data(), kRiffTag.data(), kRiffTag.size()) == 0 &&
         std::memcmp(webp.data(), kWebpTag.data(), kWebpTag.size()) == 0;
}

} // namespace iglu::textureloader::webp
