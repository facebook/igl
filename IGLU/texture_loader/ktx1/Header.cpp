/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx1/Header.h>

#include <cstring>
#include <igl/opengl/util/TextureFormat.h>

namespace iglu::textureloader::ktx1 {
namespace {
constexpr const Tag kKtx1FileIdentifier{
    {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A}};
} // namespace

igl::TextureFormatProperties Header::formatProperties() const noexcept {
  const auto format =
      igl::opengl::util::glTextureFormatToTextureFormat(glInternalFormat, glFormat, glType);
  return igl::TextureFormatProperties::fromTextureFormat(format);
}

bool Header::tagIsValid() const noexcept {
  return std::memcmp(tag.data(), kKtx1FileIdentifier.data(), kKtx1FileIdentifier.size()) == 0;
}

} // namespace iglu::textureloader::ktx1
