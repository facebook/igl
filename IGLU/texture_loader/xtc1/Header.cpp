/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/xtc1/Header.h>

#include <cstring>

namespace iglu::textureloader::xtc1 {

namespace {
constexpr const Tag kXtc1FileIdentifier{0x49, 0x56, 0x41, 0x4e};
} // namespace

bool Header::tagIsValid() const noexcept {
  return std::memcmp(magicTag.data(), kXtc1FileIdentifier.data(), kXtc1FileIdentifier.size()) == 0;
}

} // namespace iglu::textureloader::xtc1
