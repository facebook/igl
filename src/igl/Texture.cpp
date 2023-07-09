/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Texture.h>

#include <cassert>
#include <cmath>
#include <utility>

#define IGL_ENUM_TO_STRING(enum, res) \
  case enum ::res: return #res;

namespace igl {

bool isCompressedTextureFormat(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.isCompressed();
}

bool isDepthOrStencilFormat(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.isDepthOrStencil();
}

size_t toBytesPerPixel(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  if (!properties.isCompressed() && properties.isValid()) {
    return properties.bytesPerBlock;
  } else if (properties.isCompressed()) {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  } else {
    assert(0); // not implemented
    return 1;
  }
}

size_t toBytesPerBlock(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  if (properties.isCompressed()) {
    return properties.bytesPerBlock;
  } else {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  }
}

TextureBlockSize toBlockSize(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  if (properties.isCompressed()) {
    return TextureBlockSize(properties.blockWidth, properties.blockHeight);
  } else {
    IGL_ASSERT_NOT_REACHED();
    return TextureBlockSize(0, 0);
  }
}

size_t getTextureBytesPerRow(size_t texWidth, TextureFormat texFormat, size_t mipLevel) {
  const auto properties = TextureFormatProperties::fromTextureFormat(texFormat);
  size_t levelWidth = std::max(texWidth >> mipLevel, static_cast<size_t>(1));
  if (properties.isCompressed()) {
    return (levelWidth + properties.blockWidth - 1) / properties.blockWidth *
           properties.bytesPerBlock;
  } else {
    return properties.bytesPerBlock * levelWidth;
  }
}

uint32_t getTextureBytesPerSlice(size_t texWidth,
                                 size_t texHeight,
                                 size_t texDepth,
                                 TextureFormat texFormat,
                                 size_t mipLevel) {
  const auto properties = TextureFormatProperties::fromTextureFormat(texFormat);
  size_t levelWidth = std::max(texWidth >> mipLevel, static_cast<size_t>(1));
  size_t levelHeight = std::max(texHeight >> mipLevel, static_cast<size_t>(1));
  size_t levelDepth = std::max(texDepth >> mipLevel, static_cast<size_t>(1));
  if (properties.isCompressed()) {
    if (texDepth > 1) {
      assert(0);
      return 0;
    }
    const size_t widthInBlocks = (levelWidth + properties.blockWidth - 1) / properties.blockWidth;
    const size_t heightInBlocks =
        (levelHeight + properties.blockHeight - 1) / properties.blockHeight;
    return widthInBlocks * heightInBlocks * properties.bytesPerBlock;
  } else {
    return properties.bytesPerBlock * levelWidth * levelHeight * levelDepth;
  }
}

const char* textureFormatToString(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.name;
}

#define PROPERTIES(fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs) \
  case TextureFormat::fmt:                                         \
    return TextureFormatProperties{                                \
        #fmt, TextureFormat::fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs};

#define INVALID(fmt) PROPERTIES(fmt, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define COLOR(fmt, cpp, bpb, flgs) PROPERTIES(fmt, cpp, bpb, 1, 1, 1, 1, 1, 1, flgs)
#define COMPRESSED(fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs) \
  PROPERTIES(fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs | Flags::Compressed)
#define DEPTH(fmt, cpp, bpb) PROPERTIES(fmt, cpp, bpb, 1, 1, 1, 1, 1, 1, Flags::Depth)
#define DEPTH_STENCIL(fmt, cpp, bpb) \
  PROPERTIES(fmt, cpp, bpb, 1, 1, 1, 1, 1, 1, Flags::Depth | Flags::Stencil)
#define STENCIL(fmt, cpp, bpb) PROPERTIES(fmt, cpp, bpb, 1, 1, 1, 1, 1, 1, Flags::Stencil)

TextureFormatProperties TextureFormatProperties::fromTextureFormat(TextureFormat format) {
  switch (format) {
    INVALID(Invalid)
    COLOR(R_UNorm8, 1, 1, 0)
    COLOR(R_F16, 1, 2, 0)
    COLOR(R_UInt16, 1, 2, 0)
    COLOR(R_UNorm16, 1, 2, 0)
    COLOR(RG_UNorm8, 2, 2, 0)
    COLOR(RGBA_UNorm8, 4, 4, 0)
    COLOR(BGRA_UNorm8, 4, 4, 0)
    COLOR(RGBA_SRGB, 4, 4, Flags::sRGB)
    COLOR(BGRA_SRGB, 4, 4, Flags::sRGB)
    COLOR(RG_F16, 2, 4, 0)
    COLOR(RG_UInt16, 2, 4, 0)
    COLOR(RG_UNorm16, 2, 4, 0)
    COLOR(RGB10_A2_UNorm_Rev, 4, 4, 0)
    COLOR(RGB10_A2_Uint_Rev, 4, 4, 0)
    COLOR(BGR10_A2_Unorm, 4, 4, 0)
    COLOR(R_F32, 1, 4, 0)
    COLOR(RGBA_F16, 4, 8, 0)
    COLOR(RGBA_UInt32, 4, 16, 0)
    COLOR(RGBA_F32, 4, 16, 0)
    COMPRESSED(RGB8_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGB8_Punchthrough_A1_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_Punchthrough_A1_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RG_EAC_UNorm, 2, 16, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(RG_EAC_SNorm, 2, 16, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(R_EAC_UNorm, 1, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(R_EAC_SNorm, 1, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(RGBA_BC7_UNORM_4x4, 4, 16, 4, 4, 1, 1, 1, 1, 0)
    DEPTH_STENCIL(Z_UNorm16, 1, 2)
    DEPTH_STENCIL(Z_UNorm24, 1, 3)
    DEPTH_STENCIL(Z_UNorm32, 1, 4)
    DEPTH(S8_UInt_Z24_UNorm, 2, 4)
  }
  IGL_UNREACHABLE_RETURN(TextureFormatProperties{});
}

uint32_t TextureDesc::calcNumMipLevels(size_t width, size_t height) {
  if (width == 0 || height == 0) {
    return 0;
  }

  uint32_t levels = 1;

  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

Size ITexture::getSize() const {
  const auto dimensions = getDimensions();
  return Size{static_cast<float>(dimensions.width), static_cast<float>(dimensions.height)};
}

size_t ITexture::getDepth() const {
  return getDimensions().depth;
}

std::pair<Result, bool> ITexture::validateRange(const igl::TextureRangeDesc& range) const noexcept {
  if (IGL_UNEXPECTED(range.width == 0 || range.height == 0 || range.depth == 0 ||
                     range.numLayers == 0 || range.numMipLevels == 0)) {
    return std::make_pair(Result{Result::Code::ArgumentInvalid,
                                 "width, height, depth numLayers, and "
                                 "numMipLevels must be at least 1."},
                          false);
  }
  if (range.mipLevel > getNumMipLevels()) {
    return std::make_pair(
        Result{Result::Code::ArgumentOutOfRange, "range mip level exceeds texture mip levels"},
        false);
  }

  static constexpr size_t one = 1;
  const auto dimensions = getDimensions();
  const auto texWidth = std::max(dimensions.width >> range.mipLevel, one);
  const auto texHeight = std::max(dimensions.height >> range.mipLevel, one);
  const auto texDepth = std::max(dimensions.depth >> range.mipLevel, one);
  const auto texLayers = getNumLayers();

  if (range.width > texWidth || range.height > texHeight || range.depth > texDepth ||
      range.numLayers > texLayers) {
    return std::make_pair(
        Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"},
        false);
  }
  if (range.x > texWidth - range.width || range.y > texHeight - range.height ||
      range.z > texDepth - range.depth || range.layer > texLayers - range.numLayers) {
    return std::make_pair(
        Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"},
        false);
  }

  const bool fullRange = (range.x == 0 && range.y == 0 && range.z == 0 && range.layer == 0 &&
                          range.width == texWidth && range.height == texHeight &&
                          range.depth == texDepth && range.numLayers == texLayers);

  return std::make_pair(Result{}, fullRange);
}

} // namespace igl
