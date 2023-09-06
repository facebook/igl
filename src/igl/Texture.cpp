/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Texture.h>

#include <cmath>
#include <utility>

size_t std::hash<igl::TextureFormat>::operator()(igl::TextureFormat const& key) const {
  return std::hash<size_t>()(static_cast<size_t>(key));
}

namespace igl {

TextureRangeDesc TextureRangeDesc::new1D(size_t x,
                                         size_t width,
                                         size_t mipLevel,
                                         size_t numMipLevels) {
  return new3D(x, 0, 0, width, 1, 1, mipLevel, numMipLevels);
}

TextureRangeDesc TextureRangeDesc::new1DArray(size_t x,
                                              size_t width,
                                              size_t layer,
                                              size_t numLayers,
                                              size_t mipLevel,
                                              size_t numMipLevels) {
  return new2DArray(x, 0, width, 1, layer, numLayers, mipLevel, numMipLevels);
}

TextureRangeDesc TextureRangeDesc::new2D(size_t x,
                                         size_t y,
                                         size_t width,
                                         size_t height,
                                         size_t mipLevel,
                                         size_t numMipLevels) {
  return new3D(x, y, 0, width, height, 1, mipLevel, numMipLevels);
}

TextureRangeDesc TextureRangeDesc::new2DArray(size_t x,
                                              size_t y,
                                              size_t width,
                                              size_t height,
                                              size_t layer,
                                              size_t numLayers,
                                              size_t mipLevel,
                                              size_t numMipLevels) {
  auto desc = new3D(x, y, 0, width, height, 1, mipLevel, numMipLevels);
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
                                         size_t mipLevel,
                                         size_t numMipLevels) {
  TextureRangeDesc desc;
  desc.x = x;
  desc.y = y;
  desc.z = z;
  desc.width = width;
  desc.height = height;
  desc.depth = depth;
  desc.mipLevel = mipLevel;
  desc.numMipLevels = numMipLevels;
  return desc;
}

TextureRangeDesc TextureRangeDesc::newCube(size_t x,
                                           size_t y,
                                           size_t width,
                                           size_t height,
                                           size_t mipLevel,
                                           size_t numMipLevels) {
  auto desc = new3D(x, y, 0, width, height, 1, mipLevel, numMipLevels);
  desc.numFaces = 6;
  return desc;
}

TextureRangeDesc TextureRangeDesc::newCubeFace(size_t x,
                                               size_t y,
                                               size_t width,
                                               size_t height,
                                               size_t face,
                                               size_t mipLevel,
                                               size_t numMipLevels) {
  auto desc = new3D(x, y, 0, width, height, 1, mipLevel, numMipLevels);
  desc.numFaces = 1;
  desc.face = face;
  return desc;
}

TextureRangeDesc TextureRangeDesc::newCubeFace(size_t x,
                                               size_t y,
                                               size_t width,
                                               size_t height,
                                               TextureCubeFace face,
                                               size_t mipLevel,
                                               size_t numMipLevels) {
  return newCubeFace(x, y, width, height, static_cast<size_t>(face), mipLevel, numMipLevels);
}

TextureRangeDesc TextureRangeDesc::atMipLevel(size_t newMipLevel) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.numMipLevels = 1;
  newRange.mipLevel = newMipLevel;
  if (IGL_UNEXPECTED(newMipLevel < mipLevel) || newMipLevel == mipLevel) {
    return newRange;
  }

  const auto delta = newMipLevel - mipLevel;
  newRange.x = x >> delta;
  newRange.y = y >> delta;
  newRange.z = z >> delta;
  newRange.width = std::max(width >> delta, static_cast<size_t>(1));
  newRange.height = std::max(height >> delta, static_cast<size_t>(1));
  newRange.depth = std::max(depth >> delta, static_cast<size_t>(1));

  newRange.numMipLevels = 1;

  return newRange;
}

TextureRangeDesc TextureRangeDesc::withNumMipLevels(size_t newNumMipLevels) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.numMipLevels = newNumMipLevels;

  return newRange;
}

TextureRangeDesc TextureRangeDesc::atLayer(size_t newLayer) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.layer = newLayer;
  newRange.numLayers = 1;

  return newRange;
}

TextureRangeDesc TextureRangeDesc::withNumLayers(size_t newNumLayers) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.numLayers = newNumLayers;

  return newRange;
}

TextureRangeDesc TextureRangeDesc::atFace(size_t newFace) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.face = newFace;
  newRange.numFaces = 1;

  return newRange;
}

TextureRangeDesc TextureRangeDesc::atFace(TextureCubeFace newFace) const noexcept {
  return atFace(static_cast<size_t>(newFace));
}

TextureRangeDesc TextureRangeDesc::withNumFaces(size_t newNumFaces) const noexcept {
  TextureRangeDesc newRange = *this;
  newRange.numFaces = newNumFaces;

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
    COLOR(L_UNorm8, 1, 1, 0)
    COLOR(R_UNorm8, 1, 1, 0)
    COLOR(R_F16, 1, 2, 0)
    COLOR(R_UInt16, 1, 2, 0)
    COLOR(R_UNorm16, 1, 2, 0)
    COLOR(B5G5R5A1_UNorm, 4, 2, 0)
    COLOR(B5G6R5_UNorm, 3, 2, 0)
    COLOR(ABGR_UNorm4, 4, 2, 0)
    COLOR(LA_UNorm8, 2, 2, 0)
    COLOR(RG_UNorm8, 2, 2, 0)
    COLOR(R4G2B2_UNorm_Apple, 3, 2, 0)
    COLOR(R4G2B2_UNorm_Rev_Apple, 3, 2, 0)
    COLOR(R5G5B5A1_UNorm, 4, 2, 0)
    COLOR(RGBX_UNorm8, 3, 3, 0)
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
    COLOR(RGB_F16, 3, 6, 0)
    COLOR(RGBA_F16, 4, 8, 0)
    COLOR(RGB_F32, 3, 12, 0)
    COLOR(RGBA_UInt32, 4, 16, 0)
    COLOR(RGBA_F32, 4, 16, 0)
    COMPRESSED(RGBA_ASTC_4x4, 4, 16, 4, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_4x4, 4, 16, 4, 4, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_5x4, 4, 16, 5, 4, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_5x4, 4, 16, 5, 4, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_5x5, 4, 16, 5, 5, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_5x5, 4, 16, 5, 5, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_6x5, 4, 16, 6, 5, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_6x5, 4, 16, 6, 5, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_6x6, 4, 16, 6, 6, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_6x6, 4, 16, 6, 6, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_8x5, 4, 16, 8, 5, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_8x5, 4, 16, 8, 5, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_8x6, 4, 16, 8, 6, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_8x6, 4, 16, 8, 6, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_8x8, 4, 16, 8, 8, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_8x8, 4, 16, 8, 8, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_10x5, 4, 16, 10, 5, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_10x5, 4, 16, 10, 5, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_10x6, 4, 16, 10, 6, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_10x6, 4, 16, 10, 6, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_10x8, 4, 16, 10, 8, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_10x8, 4, 16, 10, 8, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_10x10, 4, 16, 10, 10, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_10x10, 4, 16, 10, 10, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_12x10, 4, 16, 12, 10, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_12x10, 4, 16, 12, 10, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_ASTC_12x12, 4, 16, 12, 12, 1, 1, 1, 1, 0)
    COMPRESSED(SRGB8_A8_ASTC_12x12, 4, 16, 12, 12, 1, 1, 1, 1, Flags::sRGB)
    COMPRESSED(RGBA_PVRTC_2BPPV1, 4, 8, 8, 4, 1, 2, 2, 1, 0)
    COMPRESSED(RGB_PVRTC_2BPPV1, 3, 8, 8, 4, 1, 2, 2, 1, 0)
    COMPRESSED(RGBA_PVRTC_4BPPV1, 4, 8, 4, 4, 1, 2, 2, 1, 0)
    COMPRESSED(RGB_PVRTC_4BPPV1, 3, 8, 4, 4, 1, 2, 2, 1, 0)
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
    COMPRESSED(RGBA_BC7_SRGB_4x4, 4, 16, 4, 4, 1, 1, 1, 1, Flags::sRGB)
    DEPTH_STENCIL(Z_UNorm16, 1, 2)
    DEPTH_STENCIL(Z_UNorm24, 1, 3)
    DEPTH_STENCIL(Z_UNorm32, 1, 4)
    DEPTH(S8_UInt_Z24_UNorm, 2, 4)
#if IGL_PLATFORM_IOS
    DEPTH(S8_UInt_Z32_UNorm, 2, 5)
#else
    DEPTH(S8_UInt_Z32_UNorm, 2, 8)
#endif
    DEPTH(S_UInt8, 1, 1)
  }
  IGL_UNREACHABLE_RETURN(TextureFormatProperties{});
}

size_t TextureFormatProperties::getRows(TextureRangeDesc range) const noexcept {
  if (range.numMipLevels == 1) {
    const size_t texHeight = std::max(range.height, static_cast<size_t>(1));
    size_t rows = texHeight;
    if (isCompressed()) {
      rows = std::max((texHeight + blockHeight - 1) / blockHeight, static_cast<size_t>(minBlocksY));
    }
    return rows * range.depth * range.numFaces * range.numLayers;
  } else {
    size_t rows = 0;
    for (size_t mipLevel = range.mipLevel; mipLevel < range.mipLevel + range.numMipLevels;
         ++mipLevel) {
      rows += getRows(range.atMipLevel(mipLevel));
    }
    return rows;
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
                                                 size_t texDepth,
                                                 size_t bytesPerRow) const noexcept {
  return getBytesPerLayer(TextureRangeDesc::new3D(0, 0, 0, texWidth, texHeight, texDepth),
                          bytesPerRow);
}

size_t TextureFormatProperties::getBytesPerLayer(TextureRangeDesc range,
                                                 size_t bytesPerRow) const noexcept {
  const auto texWidth = std::max(range.width, static_cast<size_t>(1));
  const auto texHeight = std::max(range.height, static_cast<size_t>(1));
  const auto texDepth = std::max(range.depth, static_cast<size_t>(1));
  const auto texFaces = std::max(range.numFaces, static_cast<size_t>(1));
  if (isCompressed()) {
    const size_t widthInBlocks =
        std::max((texWidth + blockWidth - 1) / blockWidth, static_cast<size_t>(minBlocksX));
    const size_t heightInBlocks =
        std::max((texHeight + blockHeight - 1) / blockHeight, static_cast<size_t>(minBlocksY));
    const size_t depthInBlocks =
        std::max((texDepth + blockDepth - 1) / blockDepth, static_cast<size_t>(minBlocksZ));
    const size_t widthBytes = std::max(bytesPerRow, widthInBlocks * bytesPerBlock);
    return texFaces * widthBytes * heightInBlocks * depthInBlocks;
  } else {
    const size_t widthBytes = std::max(bytesPerRow, texWidth * bytesPerBlock);
    return texFaces * widthBytes * texHeight * texDepth;
  }
}

size_t TextureFormatProperties::getBytesPerRange(TextureRangeDesc range,
                                                 size_t bytesPerRow) const noexcept {
  IGL_ASSERT(range.x % blockWidth == 0);
  IGL_ASSERT(range.y % blockHeight == 0);
  IGL_ASSERT(range.z % blockDepth == 0);
  IGL_ASSERT(bytesPerRow == 0 || bytesPerRow == getBytesPerRow(range) || range.numMipLevels == 1);

  size_t bytes = 0;
  for (size_t i = 0; i < range.numMipLevels; ++i) {
    bytes += getBytesPerLayer(range.atMipLevel(range.mipLevel + i), bytesPerRow) * range.numLayers;
  }

  return bytes;
}

size_t TextureFormatProperties::getNumMipLevels(size_t width,
                                                size_t height,
                                                size_t totalBytes) const noexcept {
  auto range = TextureRangeDesc::new2D(0, 0, width, height);

  size_t numMipLevels = 0;
  while (totalBytes) {
    const auto mipLevelBytes = getBytesPerRange(range.atMipLevel(numMipLevels));
    if (mipLevelBytes > totalBytes) {
      break;
    }
    totalBytes -= mipLevelBytes;
    ++numMipLevels;
  }
  return numMipLevels;
}

size_t TextureFormatProperties::getSubRangeByteOffset(const TextureRangeDesc& range,
                                                      const TextureRangeDesc& subRange,
                                                      size_t bytesPerRow) const noexcept {
  // Ensure subRange's layer, face and mipLevel range is a subset of range's.
  IGL_ASSERT(subRange.layer >= range.layer &&
             (subRange.layer + subRange.numLayers) <= (range.layer + range.numLayers));
  IGL_ASSERT(subRange.face >= range.face &&
             (subRange.face + subRange.numFaces) <= (range.face + range.numFaces));
  IGL_ASSERT(subRange.mipLevel >= range.mipLevel &&
             (subRange.mipLevel + subRange.numMipLevels) <= (range.mipLevel + range.numMipLevels));

  // Ensure subRange's dimensions are equal to the full dimensions of range's at subRange's first
  // mip level.
  IGL_ASSERT(subRange.x == range.atMipLevel(subRange.mipLevel).x &&
             subRange.width == range.atMipLevel(subRange.mipLevel).width);
  IGL_ASSERT(subRange.y == range.atMipLevel(subRange.mipLevel).y &&
             subRange.height == range.atMipLevel(subRange.mipLevel).height);
  IGL_ASSERT(subRange.z == range.atMipLevel(subRange.mipLevel).z &&
             subRange.depth == range.atMipLevel(subRange.mipLevel).depth);

  // Ensure bytes per row is either 0 OR subrange covers only the base mip level of range.
  IGL_ASSERT(bytesPerRow == 0 ||
             (subRange.mipLevel == range.mipLevel && subRange.numMipLevels == 1) ||
             bytesPerRow == getBytesPerRow(subRange));

  size_t offset = 0;
  auto workingRange = range;
  if (subRange.mipLevel > workingRange.mipLevel) {
    offset += getBytesPerRange(
        workingRange.withNumMipLevels(subRange.mipLevel - workingRange.mipLevel), bytesPerRow);
  }
  workingRange = workingRange.atMipLevel(subRange.mipLevel);
  if (subRange.layer > workingRange.layer) {
    offset += getBytesPerRange(workingRange.withNumLayers(subRange.layer - workingRange.layer),
                               bytesPerRow);
  }
  workingRange = workingRange.atLayer(subRange.layer);
  if (subRange.face > workingRange.face) {
    offset +=
        getBytesPerRange(workingRange.withNumFaces(subRange.face - workingRange.face), bytesPerRow);
  }

  return offset;
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

bool TextureDesc::operator==(const TextureDesc& rhs) const {
  return (type == rhs.type) && (format == rhs.format) && (width == rhs.width) &&
         (height == rhs.height) && (depth == rhs.depth) && (numLayers == rhs.numLayers) &&
         (numSamples == rhs.numSamples) && (usage == rhs.usage) &&
         (numMipLevels == rhs.numMipLevels) && (storage == rhs.storage) &&
         (debugName == rhs.debugName);
}

bool TextureDesc::operator!=(const TextureDesc& rhs) const {
  return !operator==(rhs);
}

float ITexture::getAspectRatio() const {
  const auto dimensions = getDimensions();
  return static_cast<float>(dimensions.width) / static_cast<float>(dimensions.height);
}

Size ITexture::getSize() const {
  const auto dimensions = getDimensions();
  return Size{static_cast<float>(dimensions.width), static_cast<float>(dimensions.height)};
}

size_t ITexture::getDepth() const {
  return getDimensions().depth;
}

size_t ITexture::getNumFaces() const {
  return getType() == TextureType::Cube ? 6 : 1;
}

size_t ITexture::getEstimatedSizeInBytes() const {
  const auto range = getFullRange(0, getNumMipLevels());
  return properties_.getBytesPerRange(range);
}

Result ITexture::validateRange(const igl::TextureRangeDesc& range) const noexcept {
  if (IGL_UNEXPECTED(range.width == 0 || range.height == 0 || range.depth == 0 ||
                     range.numLayers == 0 || range.numMipLevels == 0 || range.numFaces == 0)) {
    return Result{
        Result::Code::ArgumentInvalid,
        "width, height, depth, numLayers, numMipLevels, and numFaces must be at least 1."};
  }

  const auto dimensions = getDimensions();
  const size_t texMipLevels = getNumMipLevels();
  const size_t levelWidth = std::max(dimensions.width >> range.mipLevel, static_cast<size_t>(1));
  const size_t levelHeight = std::max(dimensions.height >> range.mipLevel, static_cast<size_t>(1));
  const size_t levelDepth = std::max(dimensions.depth >> range.mipLevel, static_cast<size_t>(1));
  const size_t texLayers = getNumLayers();
  const size_t texFaces = getNumFaces();

  if (range.width > levelWidth || range.height > levelHeight || range.depth > levelDepth ||
      range.numLayers > texLayers || range.numMipLevels > texMipLevels ||
      range.numFaces > texFaces) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }
  if (range.x > levelWidth - range.width || range.y > levelHeight - range.height ||
      range.z > levelDepth - range.depth || range.layer > texLayers - range.numLayers ||
      range.mipLevel > texMipLevels - range.numMipLevels ||
      range.face > texFaces - range.numFaces) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return Result{};
}

TextureRangeDesc ITexture::getFullRange(size_t mipLevel, size_t numMipLevels) const noexcept {
  const auto dimensions = getDimensions();

  static constexpr size_t one = 1;
  const auto texWidth = std::max(dimensions.width >> mipLevel, one);
  const auto texHeight = std::max(dimensions.height >> mipLevel, one);
  const auto texDepth = std::max(dimensions.depth >> mipLevel, one);

  auto desc = TextureRangeDesc::new3D(0, 0, 0, texWidth, texHeight, texDepth, mipLevel);
  desc.face = 0;
  desc.numLayers = getNumLayers();
  desc.numMipLevels = numMipLevels;
  desc.numFaces = getNumFaces();

  return desc;
}

TextureRangeDesc ITexture::getCubeFaceRange(size_t face,
                                            size_t mipLevel,
                                            size_t numMipLevels) const noexcept {
  IGL_ASSERT(getType() == TextureType::Cube);
  return getFullRange(mipLevel, numMipLevels).atFace(face);
}

TextureRangeDesc ITexture::getCubeFaceRange(TextureCubeFace face,
                                            size_t mipLevel,
                                            size_t numMipLevels) const noexcept {
  IGL_ASSERT(getType() == TextureType::Cube);
  return getCubeFaceRange(static_cast<size_t>(face), mipLevel, numMipLevels);
}

TextureRangeDesc ITexture::getLayerRange(size_t layer,
                                         size_t mipLevel,
                                         size_t numMipLevels) const noexcept {
  IGL_ASSERT(getType() == TextureType::TwoDArray);
  return getFullRange(mipLevel, numMipLevels).atLayer(layer);
}

} // namespace igl
