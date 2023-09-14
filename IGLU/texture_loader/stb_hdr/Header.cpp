/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/stb_hdr/Header.h>

#include <cstring>

namespace iglu::textureloader::stb::hdr {
namespace {
constexpr const Tag kRadianceFileIdentifier{
    {'#', '?', 'R', 'A', 'D', 'I', 'A', 'N', 'C', 'E', '\n'}};

constexpr const std::array<uint8_t, 7> kRgbeFileIdentifier{{'#', '?', 'R', 'G', 'B', 'E', '\n'}};
} // namespace

bool Header::tagIsValid() const noexcept {
  return std::memcmp(tag.data(), kRadianceFileIdentifier.data(), kRadianceFileIdentifier.size()) ==
             0 ||
         std::memcmp(tag.data(), kRgbeFileIdentifier.data(), kRgbeFileIdentifier.size()) == 0;
}

} // namespace iglu::textureloader::stb::hdr
