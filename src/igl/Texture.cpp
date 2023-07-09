/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Texture.h>

#include <cassert>
#include <algorithm>
#include <cmath>
#include <utility>

namespace igl {

struct TextureBlockSize {
  uint32_t width = 0;
  uint32_t height = 0;
};

bool isCompressedTextureFormat(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.isCompressed();
}

bool isDepthOrStencilFormat(TextureFormat format) {
  const auto properties = TextureFormatProperties::fromTextureFormat(format);
  return properties.isDepthOrStencil();
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

#define PROPERTIES(fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs) \
  case TextureFormat::fmt:                                         \
    return TextureFormatProperties{TextureFormat::fmt, cpp, bpb, bw, bh, bd, mbx, mby, mbz, flgs};

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

uint32_t getTextureBytesPerLayer(uint32_t texWidth,
                                 uint32_t texHeight,
                                 uint32_t texDepth,
                                 TextureFormat texFormat,
                                 uint32_t mipLevel) {
  const uint32_t levelWidth = std::max(texWidth >> mipLevel, 1u);
  const uint32_t levelHeight = std::max(texHeight >> mipLevel, 1u);
  const uint32_t levelDepth = std::max(texDepth >> mipLevel, 1u);

  const auto properties = TextureFormatProperties::fromTextureFormat(texFormat);

  if (!isCompressedTextureFormat(texFormat)) {
    return properties.bytesPerBlock * levelWidth * levelHeight * levelDepth;
  }

  if (texDepth > 1) {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  }

  const TextureBlockSize blockSize = TextureBlockSize(properties.blockWidth, properties.blockHeight);
  const uint32_t blockWidth = std::max(blockSize.width, 1u);
  const uint32_t blockHeight = std::max(blockSize.height, 1u);
  const uint32_t widthInBlocks = (levelWidth + blockWidth - 1) / blockWidth;
  const uint32_t heightInBlocks = (levelHeight + blockHeight - 1) / blockHeight;
  return widthInBlocks * heightInBlocks * properties.bytesPerBlock;
}

uint32_t TextureDesc::calcNumMipLevels(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return 0;
  }

  uint32_t levels = 1;

  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

} // namespace igl
