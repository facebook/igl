/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_jpeg/Header.h>

#include <cstring>
#include <type_traits>

static_assert(std::is_trivially_copyable_v<iglu::textureloader::stb::jpeg::Header>);

namespace iglu::textureloader::stb::jpeg {
namespace {
constexpr const Tag kJpegFileIdentifier{{0xFF, 0xD8, 0xFF}};
} // namespace

bool Header::tagIsValid() const noexcept {
  return std::memcmp(tag.data(), kJpegFileIdentifier.data(), kJpegFileIdentifier.size()) == 0;
}

} // namespace iglu::textureloader::stb::jpeg
