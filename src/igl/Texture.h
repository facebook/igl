/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

enum class ColorSpace : uint8_t {
  SRGB_LINEAR,
  SRGB_NONLINEAR,
};

enum class TextureType : uint8_t {
  TwoD,
  ThreeD,
  Cube,
};

enum SamplerFilter : uint8_t { SamplerFilter_Nearest = 0, SamplerFilter_Linear };

enum SamplerMipMap : uint8_t {
  SamplerMipMap_Disabled = 0,
  SamplerMipMap_Nearest,
  SamplerMipMap_Linear
};

enum SamplerWrapMode : uint8_t {
  SamplerWrapMode_Repeat = 0,
  SamplerWrapMode_Clamp,
  SamplerWrapMode_MirrorRepeat
};

struct SamplerStateDesc {
  SamplerFilter minFilter = SamplerFilter_Linear;
  SamplerFilter magFilter = SamplerFilter_Linear;
  SamplerMipMap mipMap = SamplerMipMap_Disabled;
  SamplerWrapMode wrapU = SamplerWrapMode_Repeat;
  SamplerWrapMode wrapV = SamplerWrapMode_Repeat;
  SamplerWrapMode wrapW = SamplerWrapMode_Repeat;
  CompareOp depthCompareOp = CompareOp_LessEqual;
  /**
   * @brief The minimum mipmap level to use when sampling a texture.
   *
   * The valid range is [0, 15]
   */
  uint8_t mipLodMin = 0;
  /**
   * @brief The maximum mipmap level to use when sampling a texture.
   *
   * The valid range is [mipLodMin, 15]
   */
  uint8_t mipLodMax = 15;
  /**
   * @brief The maximum number of samples that can be taken from a texture to improve the quality of
   * the sampled result.
   *
   * The valid range is [1, 16]
   */
  uint8_t maxAnisotropic = 1;
  bool depthCompareEnabled = false;
  const char* debugName = "";
};

class ISamplerState {
 protected:
  ISamplerState() = default;

 public:
  virtual ~ISamplerState() = default;

  virtual uint32_t getSamplerId() const = 0;
};

bool isCompressedTextureFormat(TextureFormat format);
bool isDepthOrStencilFormat(TextureFormat format);

struct TextureBlockSize {
  uint32_t width = 0;
  uint32_t height = 0;
};

/**
 * @brief Utility function to retrieve size in bytes per row in a given texture
 *
 * @param texWidth  The width of mip level 0 in the texture
 * @param texFormat Format of the texture
 * @param mipLevel  Mipmap level of the texture to calculate the bytes per row for.
 * @return Calculated total size in bytes of each row in the given texture format.
 */
size_t getTextureBytesPerRow(size_t texWidth, TextureFormat texFormat, size_t mipLevel);

struct TextureRangeDesc {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
  uint32_t width = 1;
  uint32_t height = 1;
  uint32_t depth = 1;
  uint32_t layer = 0;
  uint32_t numLayers = 1;
  uint32_t mipLevel = 0;
  uint32_t numMipLevels = 1;
};

/**
 * @brief Encapsulates properties of a texture format
 *
 *  name               - Stringified IGL enum for the format
 *  format             - IGL enum for the format
 *  componentsPerPixel - Number of components for each pixel (e.g., RGB has 3)
 *  bytesPerBlock      - Bytes per pixel block (compressed) or per pixel (uncompressed)
 *  blockWidth         - Block width for compressed textures (always 1 for uncompressed)
 *  blockHeight        - Block height for compressed textures (always 1 for uncompressed)
 *  blockDepth         - Block depth for compressed textures (always 1 for uncompressed)
 *  minBlocksX         - Minimum number of blocks in the X direction for compressed textures
 *  minBlocksY         - Minimum number of blocks in the Y direction for compressed textures
 *  minBlocksZ         - Minimum number of blocks in the Z direction for compressed textures
 *  flags              - Additional boolean flags for the format:
 *                        - Depth:      Depth texture format
 *                        - Stencil:    Stencil texture format
 *                        - Compressed: Compressed texture format
 *                        - sRGB:       sRGB texture format
 */
struct TextureFormatProperties {
  static TextureFormatProperties fromTextureFormat(TextureFormat format);

  enum Flags : uint8_t {
    Depth = 1 << 0,
    Stencil = 1 << 1,
    Compressed = 1 << 2,
    sRGB = 1 << 3,
  };

  const TextureFormat format = TextureFormat::Invalid;
  const uint8_t componentsPerPixel = 1;
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

uint32_t getTextureBytesPerLayer(uint32_t texWidth,
                                 uint32_t texHeight,
                                 uint32_t texDepth,
                                 TextureFormat texFormat,
                                 uint32_t mipLevel);

struct TextureDesc {
  /**
   * @brief Bitwise flags for texture usage
   */
  enum TextureUsageBits : uint8_t {
    Sampled = 1 << 0,
    Storage = 1 << 1,
    Attachment = 1 << 2,
  };

  using TextureUsage = uint8_t;

  TextureType type = TextureType::TwoD;
  TextureFormat format = TextureFormat::Invalid;

  uint32_t width = 1;
  uint32_t height = 1;
  uint32_t depth = 1;
  uint32_t numLayers = 1;
  uint32_t numSamples = 1;
  TextureUsage usage = 0;
  uint32_t numMipLevels = 1;
  ResourceStorage storage = ResourceStorage::Private;

  const char* debugName = "";

  static uint32_t calcNumMipLevels(uint32_t width, uint32_t height);
};

/**
 * @brief Interface class for all textures.
 */
class ITexture {
 public:
  explicit ITexture(TextureFormat format) :
    properties_(TextureFormatProperties::fromTextureFormat(format)) {}
  virtual ~ITexture() = default;

  // data[] contains per-layer mip-stacks
  virtual Result upload(const TextureRangeDesc& range, const void* data[]) const = 0;
  virtual Dimensions getDimensions() const = 0;

  [[nodiscard]] TextureFormat getFormat() const {
    return properties_.format;
  };

  virtual void generateMipmap() const = 0;

  /**
   * @brief Returns a texture id suitable for bindless rendering (descriptor indexing)
   */
  [[nodiscard]] virtual uint32_t getTextureId() const = 0;

 private:
  const TextureFormatProperties properties_;
};

} // namespace igl
