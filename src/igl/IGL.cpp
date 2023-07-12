/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <lvk/LVK.h>

#include <assert.h>
#include <algorithm>
#include <utility>

namespace igl {

bool _IGLVerify(bool cond, const char* file, int line, const char* format, ...) {
  if (!cond) {
    LLOGW("[IGL] Assert failed in %s:%d: ", file, line);
    va_list ap;
    va_start(ap, format);
    LLOGW(format, ap);
    va_end(ap);
    assert(false);
  }
  return cond;
}

struct TextureFormatProperties {
  static TextureFormatProperties fromTextureFormat(TextureFormat format);

  enum Flags : uint8_t {
    Depth = 1 << 0,
    Stencil = 1 << 1,
    Compressed = 1 << 2,
  };

  const TextureFormat format = TextureFormat::Invalid;
  const uint8_t bytesPerBlock = 1;
  const uint8_t blockWidth = 1;
  const uint8_t blockHeight = 1;
  const uint8_t blockDepth = 1;
  const uint8_t minBlocksX = 1;
  const uint8_t minBlocksY = 1;
  const uint8_t minBlocksZ = 1;
  const uint8_t flags = 0;

  [[nodiscard]] bool isCompressed() const noexcept {
    return (flags & Flags::Compressed) != 0;
  }
  [[nodiscard]] bool isDepthOrStencil() const noexcept {
    return ((flags & Flags::Depth) != 0) || (flags & Flags::Stencil) != 0;
  }
};

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

#define PROPERTIES(fmt, bpb, bw, bh, bd, mbx, mby, mbz, flgs) \
  case TextureFormat::fmt:                                         \
    return TextureFormatProperties{TextureFormat::fmt, bpb, bw, bh, bd, mbx, mby, mbz, flgs};

#define INVALID(fmt) PROPERTIES(fmt, 1, 1, 1, 1, 1, 1, 1, 0)
#define COLOR(fmt, bpb) PROPERTIES(fmt, bpb, 1, 1, 1, 1, 1, 1, 0)
#define COMPRESSED(fmt, bpb, bw, bh, bd, mbx, mby, mbz) \
  PROPERTIES(fmt, bpb, bw, bh, bd, mbx, mby, mbz, Flags::Compressed)
#define DEPTH(fmt, bpb) PROPERTIES(fmt, bpb, 1, 1, 1, 1, 1, 1, Flags::Depth)
#define DEPTH_STENCIL(fmt, bpb) \
  PROPERTIES(fmt, bpb, 1, 1, 1, 1, 1, 1, Flags::Depth | Flags::Stencil)
#define STENCIL(fmt, bpb) PROPERTIES(fmt, bpb, 1, 1, 1, 1, 1, 1, Flags::Stencil)

TextureFormatProperties TextureFormatProperties::fromTextureFormat(TextureFormat format) {

  switch (format) {
    INVALID(Invalid)
    COLOR(R_UN8, 1)
    COLOR(R_UI16, 2)
    COLOR(R_UN16, 2)
    COLOR(R_F16, 2)
    COLOR(R_F32, 4)
    COLOR(RG_UN8, 2)
    COLOR(RG_UI16, 4)
    COLOR(RG_UN16, 4)
    COLOR(RG_F16, 4)
    COLOR(RG_F32, 8)
    COLOR(RGBA_UN8, 4)
    COLOR(RGBA_UI32, 16)
    COLOR(RGBA_F16, 8)
    COLOR(RGBA_F32, 16)
    COLOR(RGBA_SRGB8, 4)
    COLOR(BGRA_UN8, 4)
    COLOR(BGRA_SRGB8, 4)
    COMPRESSED(ETC2_RGB8, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(ETC2_SRGB8, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(BC7_RGBA, 16, 4, 4, 1, 1, 1, 1)
    DEPTH(Z_UN16, 2)
    DEPTH(Z_UN24, 3)
    DEPTH(Z_F32, 4)
    DEPTH_STENCIL(Z_UN24_S_UI8, 4)
  }
#if defined(_MSC_VER)
  return TextureFormatProperties{};
#endif // _MSC_VER
}

uint32_t getTextureBytesPerLayer(uint32_t texWidth,
                                 uint32_t texHeight,
                                 uint32_t texDepth,
                                 TextureFormat texFormat,
                                 uint32_t mipLevel) {
  const uint32_t levelWidth = std::max(texWidth >> mipLevel, 1u);
  const uint32_t levelHeight = std::max(texHeight >> mipLevel, 1u);
  const uint32_t levelDepth = std::max(texDepth >> mipLevel, 1u);

  const auto props = TextureFormatProperties::fromTextureFormat(texFormat);

  if (!props.isCompressed()) {
    return props.bytesPerBlock * levelWidth * levelHeight * levelDepth;
  }

  if (texDepth > 1) {
    IGL_ASSERT_MSG(false, "Code should NOT be reached");
    return 0;
  }

  const uint32_t blockWidth = std::max((uint32_t)props.blockWidth, 1u);
  const uint32_t blockHeight = std::max((uint32_t)props.blockHeight, 1u);
  const uint32_t widthInBlocks = (levelWidth + props.blockWidth - 1) / props.blockWidth;
  const uint32_t heightInBlocks = (levelHeight + props.blockHeight - 1) / props.blockHeight;
  return widthInBlocks * heightInBlocks * props.bytesPerBlock;
}

uint32_t calcNumMipLevels(uint32_t width, uint32_t height) {
  IGL_ASSERT(width > 0);
  IGL_ASSERT(height > 0);

  uint32_t levels = 1;

  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

} // namespace igl
