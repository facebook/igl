/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Texture.h>

#include <cmath>
#include <utility>

namespace igl {

bool isCompressedTextureFormat(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.isCompressed();
}

bool isDepthOrStencilFormat(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.isDepthOrStencil();
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

size_t getTextureBytesPerSlice(size_t texWidth,
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
      IGL_ASSERT_NOT_IMPLEMENTED();
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

TextureRangeDesc TextureRangeDesc::new1D(size_t x, size_t width, size_t mipLevel) {
  return new3D(x, 0, 0, width, 1, 1, mipLevel);
}

TextureRangeDesc TextureRangeDesc::new1DArray(size_t x,
                                              size_t width,
                                              size_t layer,
                                              size_t numLayers,
                                              size_t mipLevel) {
  return new2DArray(x, 0, width, 1, layer, numLayers, mipLevel);
}

TextureRangeDesc TextureRangeDesc::new2D(size_t x,
                                         size_t y,
                                         size_t width,
                                         size_t height,
                                         size_t mipLevel) {
  return new3D(x, y, 0, width, height, 1, mipLevel);
}

TextureRangeDesc TextureRangeDesc::new2DArray(size_t x,
                                              size_t y,
                                              size_t width,
                                              size_t height,
                                              size_t layer,
                                              size_t numLayers,
                                              size_t mipLevel) {
  auto desc = new3D(x, y, 0, width, height, 1, mipLevel);
  desc.layer = layer;
  desc.numLayers = numLayers;
  return desc;
}

TextureRangeDesc TextureRangeDesc::new3D(size_t x,
                                         size_t y,
                                         size_t z,
                                         size_t width,
                                         size_t height,
                                         size_t depth,
                                         size_t mipLevel) {
  TextureRangeDesc desc;
  desc.x = x;
  desc.y = y;
  desc.z = z;
  desc.width = width;
  desc.height = height;
  desc.depth = depth;
  desc.mipLevel = mipLevel;
  return desc;
}

TextureRangeDesc TextureRangeDesc::atMipLevel(size_t newMipLevel) const noexcept {
  if (newMipLevel == mipLevel) {
    return *this;
  } else if (IGL_VERIFY(newMipLevel > mipLevel)) {
    const auto delta = newMipLevel - mipLevel;
    TextureRangeDesc newRange;
    newRange.x = x >> delta;
    newRange.y = y >> delta;
    newRange.z = z >> delta;
    newRange.width = std::max(width >> delta, static_cast<size_t>(1));
    newRange.height = std::max(height >> delta, static_cast<size_t>(1));
    newRange.depth = std::max(depth >> delta, static_cast<size_t>(1));

    newRange.layer = layer;
    newRange.numLayers = numLayers;

    newRange.mipLevel = newMipLevel;
    newRange.numMipLevels = 1;

    return newRange;
  }

  return *this;
}

TextureRangeDesc TextureRangeDesc::atLayer(size_t newLayer) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.layer = newLayer;
  newRange.numLayers = 1;

  return newRange;
}

#define PROPERTIES(fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs) \
  case TextureFormat::fmt:                                         \
    return TextureFormatProperties{                                \
        IGL_TO_STRING(fmt), TextureFormat::fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs};

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
    COLOR(A_UNorm8, 1, 1, 0)
    COLOR(R_UNorm8, 1, 1, 0)
    COLOR(R_F16, 1, 2, 0)
    COLOR(R_UInt16, 1, 2, 0)
    COLOR(R_UNorm16, 1, 2, 0)
    COLOR(B5G5R5A1_UNorm, 4, 2, 0)
    COLOR(B5G6R5_UNorm, 3, 2, 0)
    COLOR(ABGR_UNorm4, 4, 2, 0)
    COLOR(RG_UNorm8, 2, 2, 0)
    COLOR(RGBA_UNorm8, 4, 4, 0)
    COLOR(BGRA_UNorm8, 4, 4, 0)
    COLOR(BGRA_UNorm8_Rev, 4, 4, 0)
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
    COMPRESSED(RGB8_ETC1, 3, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(RGB8_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGB8_Punchthrough_A1_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_Punchthrough_A1_ETC2, 3, 8, 4, 4, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA8_EAC_ETC2, 4, 16, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_EAC_ETC2, 4, 16, 4, 4, 1, 1, 1, 1, Flags::sRGB)
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

size_t TextureFormatProperties::getRows(TextureRangeDesc range) const noexcept {
  const auto texHeight = std::max(range.height, static_cast<size_t>(1));
  if (isCompressed()) {
    const size_t heightInBlocks =
        std::max((texHeight + blockHeight - 1) / blockHeight, static_cast<size_t>(minBlocksY));
    return heightInBlocks;
  } else {
    return texHeight;
  }
}

size_t TextureFormatProperties::getBytesPerRow(size_t texWidth) const noexcept {
  return getBytesPerRow(TextureRangeDesc::new1D(0, texWidth));
}

size_t TextureFormatProperties::getBytesPerRow(TextureRangeDesc range) const noexcept {
  const auto texWidth = std::max(range.width, static_cast<size_t>(1));
  if (isCompressed()) {
    const size_t widthInBlocks =
        std::max((texWidth + blockWidth - 1) / blockWidth, static_cast<size_t>(minBlocksX));
    return widthInBlocks * bytesPerBlock;
  } else {
    return texWidth * bytesPerBlock;
  }
}

size_t TextureFormatProperties::getBytesPerLayer(size_t texWidth,
                                                 size_t texHeight,
                                                 size_t texDepth) const noexcept {
  return getBytesPerLayer(TextureRangeDesc::new3D(0, 0, 0, texWidth, texHeight, texDepth));
}

size_t TextureFormatProperties::getBytesPerLayer(TextureRangeDesc range) const noexcept {
  const auto texWidth = std::max(range.width, static_cast<size_t>(1));
  const auto texHeight = std::max(range.height, static_cast<size_t>(1));
  const auto texDepth = std::max(range.depth, static_cast<size_t>(1));
  if (isCompressed()) {
    const size_t widthInBlocks =
        std::max((texWidth + blockWidth - 1) / blockWidth, static_cast<size_t>(minBlocksX));
    const size_t heightInBlocks =
        std::max((texHeight + blockHeight - 1) / blockHeight, static_cast<size_t>(minBlocksY));
    const size_t depthInBlocks =
        std::max((texDepth + blockDepth - 1) / blockDepth, static_cast<size_t>(minBlocksZ));
    return widthInBlocks * heightInBlocks * depthInBlocks * bytesPerBlock;
  } else {
    return texWidth * texHeight * texDepth * bytesPerBlock;
  }
}

size_t TextureFormatProperties::getBytesPerRange(TextureRangeDesc range) const noexcept {
  IGL_ASSERT(range.x % blockWidth == 0);
  IGL_ASSERT(range.y % blockHeight == 0);
  IGL_ASSERT(range.z % blockDepth == 0);

  size_t bytes = 0;
  for (size_t i = 0; i < range.numMipLevels; ++i) {
    bytes += getBytesPerLayer(range.atMipLevel(range.mipLevel + i)) * range.numLayers;
  }

  return bytes;
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

size_t ITexture::getEstimatedSizeInBytes() const {
  const auto range = getFullRange(0, isRequiredGenerateMipmap() ? getNumMipLevels() : 1);
  auto totalBytes = properties_.getBytesPerRange(range);

  if (getType() == igl::TextureType::Cube) {
    totalBytes *= 6;
  }

  return totalBytes;
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

TextureRangeDesc ITexture::getFullRange(size_t mipLevel, size_t numMipLevels) const noexcept {
  const auto dimensions = getDimensions();

  static constexpr size_t one = 1;
  const auto texWidth = std::max(dimensions.width >> mipLevel, one);
  const auto texHeight = std::max(dimensions.height >> mipLevel, one);
  const auto texDepth = std::max(dimensions.depth >> mipLevel, one);

  auto desc = TextureRangeDesc::new3D(0, 0, 0, texWidth, texHeight, texDepth, mipLevel);
  desc.numLayers = getNumLayers();
  desc.numMipLevels = numMipLevels;

  return desc;
}

} // namespace igl
