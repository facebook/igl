/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_png/Header.h>

#include <cstring>

namespace iglu::textureloader::stb::png {
namespace {
constexpr const Tag kPngFileIdentifier{{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a}};
} // namespace

bool Header::tagIsValid() const noexcept {
  return std::memcmp(tag.data(), kPngFileIdentifier.data(), kPngFileIdentifier.size()) == 0;
}

} // namespace iglu::textureloader::stb::png
