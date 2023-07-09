/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <algorithm>
#include <igl/Common.h>

namespace igl {

class IDevice;

enum SamplerMinMagFilter : uint8_t { SamplerMinMagFilter_Nearest = 0, SamplerMinMagFilter_Linear };

enum SamplerMipFilter : uint8_t {
  SamplerMipFilter_Disabled = 0,
  SamplerMipFilter_Nearest,
  SamplerMipFilter_Linear
};

enum SamplerAddressMode : uint8_t {
  SamplerAddressMode_Repeat = 0,
  SamplerAddressMode_Clamp,
  SamplerAddressMode_MirrorRepeat
};

struct SamplerStateDesc {
  SamplerMinMagFilter minFilter = SamplerMinMagFilter_Linear;
  SamplerMinMagFilter magFilter = SamplerMinMagFilter_Linear;
  SamplerMipFilter mipFilter = SamplerMipFilter_Disabled;
  SamplerAddressMode addressModeU = SamplerAddressMode_Repeat;
  SamplerAddressMode addressModeV = SamplerAddressMode_Repeat;
  SamplerAddressMode addressModeW = SamplerAddressMode_Repeat;
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
  std::string debugName = "";
};

class ISamplerState {
 protected:
  ISamplerState() = default;

 public:
  virtual ~ISamplerState() = default;

  virtual uint32_t getSamplerId() const = 0;
};

/**
 *
 * The list of different color space commonly seen in graphics.
 *
 */
enum class ColorSpace : uint8_t {
  SRGB_LINEAR,
  SRGB_NONLINEAR,
};

enum class TextureType : uint8_t {
  TwoD,
  ThreeD,
  Cube,
};

bool isCompressedTextureFormat(TextureFormat format);
bool isDepthOrStencilFormat(TextureFormat format);

/**
 * @brief Converts texture format to literal string
 *
 * @param format Format of the texture
 * @return Literal C-style string containing the texture format
 */
const char* textureFormatToString(TextureFormat format);

/**
 * @brief Converts texture format to bytes per pixel
 *
 * @param format Format of the texture
 * @return Number of bytes containing per pixel
 */
size_t toBytesPerPixel(TextureFormat format);

/**
 * @brief Converts texture format to bytes per block (Compressed Usage)
 *
 * @param format Format of the texture
 * @return Number of bytes containing per pixel in each block
 */
size_t toBytesPerBlock(TextureFormat format);

/**
 * @brief POD for texture block size
 */
struct TextureBlockSize {
  uint32_t width = 0;
  uint32_t height = 0;

  TextureBlockSize(uint32_t w, uint32_t h) {
    width = w;
    height = h;
  }
};

TextureBlockSize toBlockSize(TextureFormat format);

/**
 * @brief Utility function to retrieve size in bytes per row in a given texture
 *
 * @param texWidth  The width of mip level 0 in the texture
 * @param texFormat Format of the texture
 * @param mipLevel  Mipmap level of the texture to calculate the bytes per row for.
 * @return Calculated total size in bytes of each row in the given texture format.
 */
size_t getTextureBytesPerRow(size_t texWidth, TextureFormat texFormat, size_t mipLevel);

/**
 * @brief Utility function to retrieve size in bytes per slice in a given texture
 *
 * @param texWidth  The width of mip level 0 in the texture
 * @param texHeight The height of mip level 0 in the texture
 * @param texDepth  The depth of mip level 0 in the texture
 * @param texFormat Format of the texture
 * @param mipLevel  Mipmap level of the texture to calculate the bytes per slice for.
 * @return Calculated total size in bytes of each row in the given texture format.
 */
uint32_t getTextureBytesPerSlice(size_t texWidth,
                                 size_t texHeight,
                                 size_t texDepth,
                                 TextureFormat texFormat,
                                 size_t mipLevel);

struct TextureRangeDesc {
  size_t x = 0;
  size_t y = 0;
  size_t z = 0;
  size_t width = 1;
  size_t height = 1;
  size_t depth = 1;
  size_t layer = 0;
  size_t numLayers = 1;
  size_t mipLevel = 0;
  size_t numMipLevels = 1;
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

  const char* name = "Invalid";
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

  /**
   * @brief true for TextureFormat::Invalid
   */
  [[nodiscard]] bool isValid() const noexcept {
    return format != TextureFormat::Invalid;
  }
  /**
   * @brief true compressed texture formats.
   */
  [[nodiscard]] bool isCompressed() const noexcept {
    return (flags & Flags::Compressed) != 0;
  }
  /**
   * @brief true sRGB texture formats.
   */
  [[nodiscard]] bool isSRGB() const noexcept {
    return (flags & Flags::sRGB) != 0;
  }
  /**
   * @brief true depth-only texture formats (e.g., TextureFormat::Z_UNorm24).
   */
  [[nodiscard]] bool isDepthOnly() const noexcept {
    return (flags & Flags::Depth) != 0 && (flags & Flags::Stencil) == 0;
  }
  /**
   * @brief true stencil-only texture formats (e.g., TextureFormat::S_UInt8).
   */
  [[nodiscard]] bool isStencilOnly() const noexcept {
    return (flags & Flags::Depth) == 0 && (flags & Flags::Stencil) != 0;
  }
  /**
   * @brief true depth-only, stencil-only and depth-stencil texture formats.
   */
  [[nodiscard]] bool isDepthOrStencil() const noexcept {
    return (flags & Flags::Depth) != 0 || (flags & Flags::Stencil) != 0;
  }

  /**
   * @brief Utility function to calculate the size in bytes per texture slice for a texture format.
   *
   * @param texWidth  The width of the texture layer.
   * @param texHeight  The height of the texture layer.
   * @param texDepth  The depth of the texture layer.
   * @remark texWidth, texHeight and texDepth should be the actual dimensions of the range to
   * calculate for. For subranges and mip levels other than 0, this should be the dimensions of the
   * subrange and/or mip level, which may be be less than the full texture dimensions.
   * @return Calculated total size in bytes of a layer of texture data for the texture format.
   */
  [[nodiscard]] size_t getBytesPerLayer(size_t texWidth,
                                        size_t texHeight,
                                        size_t texDepth) const noexcept;

  /**
   * @brief Utility function to calculate the size in bytes per texture layer for a texture format.
   *
   * @param range  The range of the texture to calculate for. The range should be the full size of
   * the first mip level to calculate for. range.x, range.y, range.z, range.mipLevel and range.layer
   * are not used.
   * @remark range.width, range.height, and range.depth should be the actual dimensions of the range
   * to calculate for. For subranges and mip levels other than 0, these should be the dimensions of
   * the subrange and/or mip level, which may be be less than the full texture dimensions.
   * @return Calculated total size in bytes of a layer of texture data for the texture format.
   */
  size_t getBytesPerLayer(TextureRangeDesc range) const noexcept;

  /**
   * @brief Utility function to calculate the size in bytes per texture range for a texture format.
   *
   * @param range  The range of the texture to calculate for. The range should be the full size of
   * the first mip level to calculate for. range.x, range.y and range.z are not used.
   * @remark range can include more than one layer. range can also include more than one mip level.
   * When range includes more than one mip level, dimensions  are divided by two for each subsequent
   * mip level.
   * @return Calculated total size in bytes of a the range of texture data for the texture format.
   */
  [[nodiscard]] size_t getBytesPerRange(TextureRangeDesc range) const noexcept;
};

/**
 * @brief TextureCubeFace denotes side of the face in a cubemap setting.
 * Based on https://www.khronos.org/opengl/wiki/Cubemap_Texture
 */
enum class TextureCubeFace : uint8_t {
  PosX = 0,
  NegX,
  PosY,
  NegY,
  PosZ,
  NegZ,
};

struct TextureDesc {
  /**
   * @brief Bitwise flags for texture usage
   *
   *  Sampled - Can be used as read-only texture in vertex/fragment shaders
   *  Storage - Can be used as read/write storage texture in vertex/fragment/compute shaders
   *  Attachment - Can be bound for render target
   */
  enum TextureUsageBits : uint8_t {
    Sampled = 1 << 0,
    Storage = 1 << 1,
    Attachment = 1 << 2,
  };

  using TextureUsage = uint8_t;

  uint32_t width = 1;
  uint32_t height = 1;
  uint32_t depth = 1;
  uint32_t numLayers = 1;
  uint32_t numSamples = 1;
  TextureUsage usage = 0;
  uint32_t numMipLevels = 1;
  TextureType type = TextureType::TwoD;
  TextureFormat format = TextureFormat::Invalid;
  ResourceStorage storage = ResourceStorage::Private;

  std::string debugName = "";

  static TextureDesc new2D(TextureFormat format,
                           uint32_t width,
                           uint32_t height,
                           TextureUsage usage,
                           const char* debugName = nullptr) {
    return TextureDesc{width,
                       height,
                       1,
                       1,
                       1,
                       usage,
                       1,
                       TextureType::TwoD,
                       format,
                       ResourceStorage::Private,
                       debugName ? debugName : ""};
  }

  static TextureDesc newCube(TextureFormat format,
                             size_t width,
                             size_t height,
                             TextureUsage usage,
                             const char* debugName = nullptr) {
    return TextureDesc{(uint32_t)width,
                       (uint32_t)height,
                       1,
                       1,
                       1,
                       usage,
                       1,
                       TextureType::Cube,
                       format,
                       ResourceStorage::Private,
                       debugName ? debugName : ""};
  }

  static uint32_t calcNumMipLevels(size_t width, size_t height);
};

/**
 * @brief Interface class for all textures.
 * This should only be used for the purpose of getting information about the texture using the
 * gettor methods defined below.
 */
class ITexture {
 public:
  explicit ITexture(TextureFormat format) :
    properties_(TextureFormatProperties::fromTextureFormat(format)) {}
  virtual ~ITexture() = default;

  /**
   * @brief Uploads the given data into texture memory.
   *
   * @param range        The texture range descriptor containing the offset & dimensions of the
   * upload process.
   * @param data         The pointer to the data. May be a nullptr to force initialization without
   * providing data.
   * @param bytesPerRow Number of bytes per row. If 0, it will be autocalculated assuming no
   * padding.
   * @return Result      A flag for the result of operation
   */
  virtual Result upload(const TextureRangeDesc& range,
                        const void* data,
                        size_t bytesPerRow = 0) const = 0;

  /**
   * @brief Uploads the given data into texture memory in a given cube map face
   *
   * @param range       The texture range descriptor containing the offset & dimensions of the
   * upload process.
   * @param face        The cube face map index
   * @param data        The pointer to the data. May be a nullptr to force initialization without
   * providing data.
   * @param bytesPerRow Number of bytes per row. If 0, it will be autocalculated assuming no
   * padding.
   * @return Result     A flag for the result of operation
   */
  virtual Result uploadCube(const TextureRangeDesc& range,
                            TextureCubeFace face,
                            const void* data,
                            size_t bytesPerRow) const = 0;
  /**
   * @brief Returns size (width x height) dimension of the texture.
   * For 1D textures, return (width,1)
   * For 2D textures, 3D textures and/or cube, return (width,height)
   *
   * @return Size
   */
  [[nodiscard]] Size getSize() const;
  /**
   * @brief Returns depth dimension of the texture
   * For 1D, 2D textures, return 1
   * For 3D textures and/or cube, returns depth
   *
   * @return size_t
   */
  [[nodiscard]] size_t getDepth() const;
  /**
   * @brief Returns dimensions (width, height and depth) of the texture.
   *
   * @return size_t
   */
  [[nodiscard]] virtual Dimensions getDimensions() const = 0;
  /**
   * @brief Returns the number of layers of the texture
   * For non array textures, return 1
   *
   * @return size_t
   */
  [[nodiscard]] virtual size_t getNumLayers() const = 0;

  [[nodiscard]] TextureFormat getFormat() const {
    return properties_.format;
  }
  /**
   * @brief Returns texture format properties of the texture
   *
   * @return TextureFormatProperties
   */
  [[nodiscard]] TextureFormatProperties getProperties() const {
    return properties_;
  }
  /**
   * @brief Returns texture type of the texture
   *
   * @return TextureType
   */
  [[nodiscard]] virtual TextureType getType() const = 0;
  /**
   * @brief Returns bitwise flag containing the usage of the texture
   *
   * @return unsigned bitwise flag
   */
  [[nodiscard]] virtual uint32_t getUsage() const = 0;
  /**
   * @brief Returns number of samples
   *
   * @return The count of samples in the underlying texture
   */
  [[nodiscard]] virtual size_t getSamples() const = 0;
  /**
   * @brief Generates mipmap command using the command queue
   */
  virtual void generateMipmap() const = 0;
  /**
   * @brief Returns the number of mipmap levels
   */
  [[nodiscard]] virtual size_t getNumMipLevels() const = 0;
  /**
   * @brief Returns number of bytes per pixel in the underlying textures
   *
   * @return size_t
   */
  [[nodiscard]] inline size_t getBytesPerPixel() const {
    return properties_.bytesPerBlock;
  }
  /**
   * @brief Returns a texture id suitable for bindless rendering (descriptor indexing on Vulkan and
   * gpuResourceID on Metal)
   */
  [[nodiscard]] virtual uint32_t getTextureId() const = 0;

  /**
   * @brief Validates the range against texture dimensions at the range's mip level.
   *
   * @return The returned Result indicates whether the range is valid or not. The returned bool is
   * true if the range covers the full texture at the range's mipLevel and false otherwise.
   */
  [[nodiscard]] std::pair<Result, bool> validateRange(
      const igl::TextureRangeDesc& range) const noexcept;

 private:
  const TextureFormatProperties properties_;
};

/**
 * @brief Holds textures associated with an externally owned surface (e.g., a window).
 */
struct SurfaceTextures {
  /** @brief The surface's color texture. */
  std::shared_ptr<igl::ITexture> color;
  /** @brief The surface's depth texture. */
  std::shared_ptr<igl::ITexture> depth;
};

} // namespace igl
