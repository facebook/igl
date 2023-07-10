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

bool _IGLVerify(bool cond, const char* func, const char* file, int line, const char* format, ...) {
  if (!cond) {
    LLOGW("[IGL] Assert failed in '%s' (%s:%d): ", func, file, line);
    va_list ap;
    va_start(ap, format);
    LLOGW(format, ap);
    va_end(ap);
    assert(false);
  }
  return cond;
}

struct TextureBlockSize {
  uint32_t width = 0;
  uint32_t height = 0;
};

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
    COLOR(R_UNorm8, 1)
    COLOR(R_F16, 2)
    COLOR(R_UInt16, 2)
    COLOR(R_UNorm16, 2)
    COLOR(RG_UNorm8, 2)
    COLOR(RGBA_UNorm8, 4)
    COLOR(BGRA_UNorm8, 4)
    COLOR(RGBA_SRGB, 4)
    COLOR(BGRA_SRGB, 4)
    COLOR(RG_F16, 4)
    COLOR(RG_UInt16, 4)
    COLOR(RG_UNorm16, 4)
    COLOR(RGB10_A2_UNorm_Rev, 4)
    COLOR(RGB10_A2_Uint_Rev, 4)
    COLOR(BGR10_A2_Unorm, 4)
    COLOR(R_F32, 4)
    COLOR(RGBA_F16, 8)
    COLOR(RGBA_UInt32, 16)
    COLOR(RGBA_F32, 16)
    COMPRESSED(RGB8_ETC2, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(SRGB8_ETC2, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(RGB8_Punchthrough_A1_ETC2, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(SRGB8_Punchthrough_A1_ETC2, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(RG_EAC_UNorm, 16, 4, 4, 1, 1, 1, 1)
    COMPRESSED(RG_EAC_SNorm, 16, 4, 4, 1, 1, 1, 1)
    COMPRESSED(R_EAC_UNorm, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(R_EAC_SNorm, 8, 4, 4, 1, 1, 1, 1)
    COMPRESSED(RGBA_BC7_UNORM_4x4, 16, 4, 4, 1, 1, 1, 1)
    DEPTH(Z_UNorm16, 2)
    DEPTH(Z_UNorm24, 3)
    DEPTH(Z_UNorm32, 4)
    DEPTH_STENCIL(S8_UInt_Z24_UNorm, 4)
  }
#if defined(_MSC_VER)
  IGL_ASSERT_NOT_REACHED();
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

  const auto properties = TextureFormatProperties::fromTextureFormat(texFormat);

  if (!properties.isCompressed()) {
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

uint32_t calcNumMipLevels(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return 0;
  }

  uint32_t levels = 1;

  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

ShaderStages IDevice::createShaderStages(const char* vs,
                                         const char* debugNameVS,
                                         const char* fs,
                                         const char* debugNameFS,
                                         Result* outResult) {
  auto VS = createShaderModule(igl::ShaderModuleDesc(vs, Stage_Vertex, debugNameVS), outResult);
  auto FS = createShaderModule(igl::ShaderModuleDesc(fs, Stage_Fragment, debugNameFS), outResult);
  return igl::ShaderStages(VS, FS);
}

} // namespace igl
