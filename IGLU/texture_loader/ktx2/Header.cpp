/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx2/Header.h>

#include <cstring>

namespace iglu::textureloader::ktx2 {
namespace {
constexpr const Tag kKtx2FileIdentifier{
    {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A}};
} // namespace

bool Header::tagIsValid() const noexcept {
  return std::memcmp(tag.data(), kKtx2FileIdentifier.data(), kKtx2FileIdentifier.size()) == 0;
}

} // namespace iglu::textureloader::ktx2
