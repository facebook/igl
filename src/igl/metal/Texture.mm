/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/Texture.h>

#include <vector>

namespace {

/// Metal textures are up-side down compared to OGL textures. IGL follows
/// the OGL convention and this function flips the texture vertically
void flipBMP(unsigned char* dstImg, unsigned char* srcImg, size_t height, size_t bytesPerRow) {
  for (size_t h = 0; h < height; h++) {
    memcpy(dstImg + h * bytesPerRow, srcImg + bytesPerRow * (height - 1 - h), bytesPerRow);
  }
}

void bgrToRgb(unsigned char* dstImg, size_t width, size_t height, size_t bytesPerPixel) {
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      auto pixelIndex = i * width + j;
      std::swap(dstImg[pixelIndex * bytesPerPixel + 0], dstImg[pixelIndex * bytesPerPixel + 2]);
    }
  }
}

/// returns true if the GPU can filter a texture with the given format during sampling
/// referenced from table "Texture capabilities by pixel format" in:
/// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
///
/// no filtering for:
///   - int textures: 8/16/32 UInt/SInt
///   - 32-bit float textures: 32Float (unless device supports it)
///   - depth/stencil (except for Depth16Unorm)
bool isFilterable(MTLPixelFormat format, bool supports32BitFloatFiltering) {
  switch (format) {
  // int textures: 8/16/32 UInt/SInt
  case MTLPixelFormatR8Uint:
  case MTLPixelFormatR8Sint:
  case MTLPixelFormatRG8Uint:
  case MTLPixelFormatRG8Sint:
  case MTLPixelFormatRGBA8Uint:
  case MTLPixelFormatRGBA8Sint:

  case MTLPixelFormatR16Uint:
  case MTLPixelFormatR16Sint:
  case MTLPixelFormatRG16Uint:
  case MTLPixelFormatRG16Sint:
  case MTLPixelFormatRGBA16Uint:
  case MTLPixelFormatRGBA16Sint:

  case MTLPixelFormatR32Uint:
  case MTLPixelFormatR32Sint:
  case MTLPixelFormatRG32Uint:
  case MTLPixelFormatRG32Sint:
  case MTLPixelFormatRGBA32Uint:
  case MTLPixelFormatRGBA32Sint:
    return false;

  // 32-bit float textures: 32Float (unless device supports it)
  case MTLPixelFormatR32Float:
  case MTLPixelFormatRG32Float:
  case MTLPixelFormatRGBA32Float:
    return supports32BitFloatFiltering;

  // depth/stencil (except for Depth16Unorm)
  case MTLPixelFormatStencil8:
  case MTLPixelFormatDepth32Float_Stencil8:
  case MTLPixelFormatX32_Stencil8:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
  case MTLPixelFormatDepth24Unorm_Stencil8:
  case MTLPixelFormatX24_Stencil8:
#endif
    return false;

  // misc
  case MTLPixelFormatInvalid:
    return false;

  default:
    // all other formats support filtering
    return true;
  }
}
} // namespace

namespace igl {
namespace metal {

Texture::Texture(id<MTLTexture> texture) :
  ITexture(mtlPixelFormatToTextureFormat([texture pixelFormat])),
  value_(texture),
  drawable_(nullptr) {}

Texture::Texture(id<CAMetalDrawable> drawable) :
  ITexture(mtlPixelFormatToTextureFormat([drawable.texture pixelFormat])),
  value_(nullptr),
  drawable_(drawable) {}

Texture::~Texture() {
  value_ = nil;
}

Result Texture::upload(const TextureRangeDesc& range, const void* data, size_t bytesPerRow) const {
  if (range.numMipLevels > 1) {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return Result(Result::Code::Unimplemented, "Can't upload more than 1 mip-level");
  }
  if (data == nullptr) {
    return Result(Result::Code::Ok);
  }
  const auto [result, _] = validateRange(range);
  if (!result.isOk()) {
    return result;
  }
  if (bytesPerRow == 0) {
    bytesPerRow = getProperties().getBytesPerRow(range);
  }
  const auto numLayers = std::max(range.numLayers, static_cast<size_t>(1));
  const auto byteIncrement = numLayers > 1 ? getProperties().getBytesPerLayer(range.atLayer(0)) : 0;
  for (auto i = 0; i < numLayers; ++i) {
    MTLRegion region;
    switch (getType()) {
    case TextureType::TwoD:
    case TextureType::TwoDArray:
      region = MTLRegionMake2D(range.x, range.y, range.width, range.height);
      [get() replaceRegion:region
               mipmapLevel:range.mipLevel
                     slice:range.layer + i
                 withBytes:data
               bytesPerRow:toMetalBytesPerRow(bytesPerRow)
             bytesPerImage:0];
      break;
    case TextureType::ThreeD:
      region = MTLRegionMake3D(range.x, range.y, range.z, range.width, range.height, range.depth);
      [get() replaceRegion:region
               mipmapLevel:range.mipLevel
                     slice:0 /* 3D array textures not supported */
                 withBytes:data
               bytesPerRow:toMetalBytesPerRow(bytesPerRow)
             bytesPerImage:toMetalBytesPerRow(bytesPerRow * range.height)];
      break;
    default:
      IGL_ASSERT(false && "Unknown texture type");
      break;
    }

    data = static_cast<const uint8_t*>(data) + byteIncrement;
  }
  return Result(Result::Code::Ok);
}

Result Texture::getBytes(const TextureRangeDesc& range, void* outData, size_t bytesPerRow) const {
  if (!outData) {
    return Result(Result::Code::ArgumentNull, "Need a valid output buffer");
  }
  if (get().storageMode == MTLStorageModePrivate) {
    return Result(
        Result::Code::Unsupported,
        "Can't retrieve the data from private memory; use a blit command encoder instead");
  }

  size_t bytesPerImage = getProperties().getBytesPerRange(range);
  MTLRegion region = {{range.x, range.y, 0}, {range.width, range.height, 1}};
  auto tmpBuffer = std::vector<uint8_t>(bytesPerImage);

  [get() getBytes:tmpBuffer.data()
        bytesPerRow:bytesPerRow
      bytesPerImage:bytesPerImage
         fromRegion:region
        mipmapLevel:range.mipLevel
              slice:range.layer];

  flipBMP(static_cast<unsigned char*>(outData), tmpBuffer.data(), range.height, bytesPerRow);

  igl::TextureFormat f = getFormat();
  TextureFormatProperties props = TextureFormatProperties::fromTextureFormat(f);
  auto bytesPerPixel = props.bytesPerBlock;
  if (isTextureFormatBGR(f)) {
    bgrToRgb(static_cast<unsigned char*>(outData), range.width, range.height, bytesPerPixel);
  }
  return Result(Result::Code::Ok);
}

size_t Texture::toMetalBytesPerRow(size_t bytesPerRow) const {
  switch (getFormat()) {
  case TextureFormat::RGBA_PVRTC_2BPPV1:
  case TextureFormat::RGB_PVRTC_2BPPV1:
  case TextureFormat::RGBA_PVRTC_4BPPV1:
  case TextureFormat::RGB_PVRTC_4BPPV1:
    return 0;
  default:
    return bytesPerRow;
  }
}

Result Texture::uploadCube(const TextureRangeDesc& range,
                           TextureCubeFace face,
                           const void* data,
                           size_t bytesPerRow) const {
  if (getType() != TextureType::Cube) {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return Result(Result::Code::Unsupported);
  }
  if (range.numMipLevels > 1) {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return Result(Result::Code::Unimplemented, "Can't upload more than 1 mip-level");
  }
  const auto [result, _] = validateRange(range);
  if (!result.isOk()) {
    return result;
  }
  if (bytesPerRow == 0) {
    bytesPerRow = getProperties().getBytesPerRow(range);
  }
  if (data) {
    MTLRegion region = MTLRegionMake2D(range.x, range.y, range.width, range.height);
    int layer = static_cast<int>(face);
    [get() replaceRegion:region
             mipmapLevel:range.mipLevel
                   slice:layer
               withBytes:data
             bytesPerRow:toMetalBytesPerRow(bytesPerRow)
           bytesPerImage:0];
  }
  return Result(Result::Code::Ok);
}

Dimensions Texture::getDimensions() const {
  auto texture = get();
  return Dimensions{[texture width], [texture height], [texture depth]};
}

size_t Texture::getNumLayers() const {
  return [get() arrayLength];
}

TextureType Texture::getType() const {
  return convertType([get() textureType]);
}

ulong_t Texture::getUsage() const {
  return toTextureUsage([get() usage]);
}

size_t Texture::getSamples() const {
  return [get() sampleCount];
}

size_t Texture::getNumMipLevels() const {
  return [get() mipmapLevelCount];
}

void Texture::generateMipmap(ICommandQueue& cmdQueue) const {
  if (value_.mipmapLevelCount > 1) {
    auto mtlCmdQueue = static_cast<CommandQueue&>(cmdQueue).get();

    // we can only generate mipmaps for filterable texture formats via the blit encoder
    bool supports32BitFloatFiltering = false;
    if (@available(macOS 11.0, iOS 14.0, *)) {
      // this API became available as of iOS 14 and macOS 11
      supports32BitFloatFiltering = mtlCmdQueue.device.supports32BitFloatFiltering;
    }
    if (!isFilterable(value_.pixelFormat, supports32BitFloatFiltering)) {
      // TODO: implement manual mip generation for required formats (e.g. RGBA32Float)
      IGL_ASSERT_NOT_IMPLEMENTED();
      return;
    }

    id<MTLCommandBuffer> buffer = [mtlCmdQueue commandBuffer];
    id<MTLBlitCommandEncoder> encoder = [buffer blitCommandEncoder];
    [encoder generateMipmapsForTexture:value_];
    [encoder endEncoding];
    [buffer commit];
  }
}

bool Texture::isRequiredGenerateMipmap() const {
  return value_.mipmapLevelCount > 1;
}

uint64_t Texture::getTextureId() const {
  // TODO: implement via gpuResourceID
  IGL_ASSERT_NOT_IMPLEMENTED();
  return 0;
}

ulong_t Texture::toTextureUsage(MTLTextureUsage usage) {
  ulong_t result = 0;
  result |= ((usage & MTLTextureUsageShaderRead) != 0) ? TextureDesc::TextureUsageBits::Sampled : 0;
  result |= ((usage & MTLTextureUsageShaderWrite) != 0) ? TextureDesc::TextureUsageBits::Storage
                                                        : 0;
  result |= ((usage & MTLTextureUsageRenderTarget) != 0) ? TextureDesc::TextureUsageBits::Attachment
                                                         : 0;
  return result;
}

MTLTextureUsage Texture::toMTLTextureUsage(ulong_t usage) {
  MTLTextureUsage result = 0;
  result |= ((usage & TextureDesc::TextureUsageBits::Sampled) != 0) ? MTLTextureUsageShaderRead : 0;
  result |= ((usage & TextureDesc::TextureUsageBits::Storage) != 0) ? MTLTextureUsageShaderWrite
                                                                    : 0;
  result |= ((usage & TextureDesc::TextureUsageBits::Attachment) != 0) ? MTLTextureUsageRenderTarget
                                                                       : 0;
  return result;
}

MTLTextureType Texture::convertType(TextureType value, size_t numSamples) {
  IGL_ASSERT(value != TextureType::Invalid && value != TextureType::ExternalImage);

  switch (value) {
  case TextureType::ExternalImage:
  case TextureType::Invalid:
    return MTLTextureType1D; // Use 1D texture as fallback for invalid
  case TextureType::TwoD:
    return numSamples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
  case TextureType::TwoDArray:
    if (@available(macOS 10.14, iOS 14.0, *)) {
      return numSamples > 1 ? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
    } else {
      return MTLTextureType2DArray;
    }
  case TextureType::ThreeD:
    return MTLTextureType3D;
  case TextureType::Cube:
    return MTLTextureTypeCube;
  }
}

TextureType Texture::convertType(MTLTextureType value) {
  switch (value) {
  case MTLTextureType2D:
  case MTLTextureType2DMultisample:
    return TextureType::TwoD;
  case MTLTextureType2DArray:
    return TextureType::TwoDArray;
  case MTLTextureType3D:
    return TextureType::ThreeD;
  case MTLTextureTypeCube:
    return TextureType::Cube;
  default:
    if (@available(macOS 10.14, iOS 14.0, *)) {
      if (value == MTLTextureType2DMultisampleArray) {
        return TextureType::TwoDArray;
      }
    }
    return TextureType::Invalid;
  }
}

MTLPixelFormat Texture::textureFormatToMTLPixelFormat(TextureFormat value) {
  switch (value) {
  case TextureFormat::Invalid:
    return MTLPixelFormatInvalid;

    // 8 bpp
  case TextureFormat::R_UNorm8:
    return MTLPixelFormatR8Unorm;
  case TextureFormat::A_UNorm8:
    return MTLPixelFormatA8Unorm;
    // 16 bpp
  case TextureFormat::B5G5R5A1_UNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatBGR5A1Unorm;
#endif
  case TextureFormat::B5G6R5_UNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST || IGL_PLATFORM_IOS_SIMULATOR
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatB5G6R5Unorm;
#endif
  case TextureFormat::ABGR_UNorm4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST || IGL_PLATFORM_IOS_SIMULATOR
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatABGR4Unorm;
#endif
  case TextureFormat::RG_UNorm8:
    return MTLPixelFormatRG8Unorm;
  case TextureFormat::R4G2B2_UNorm_Apple:
    return MTLPixelFormatBGRG422;
  case TextureFormat::R4G2B2_UNorm_Rev_Apple:
    return MTLPixelFormatGBGR422;

  case TextureFormat::R_F16:
    return MTLPixelFormatR16Float;
  case TextureFormat::R_UInt16:
    return MTLPixelFormatR16Uint;
  case TextureFormat::R_UNorm16:
    return MTLPixelFormatR16Unorm;

    // 32 bpp
  case TextureFormat::RGBA_UNorm8:
    return MTLPixelFormatRGBA8Unorm;
  case TextureFormat::BGRA_UNorm8:
    return MTLPixelFormatBGRA8Unorm;

  case TextureFormat::RGBA_SRGB:
    return MTLPixelFormatRGBA8Unorm_sRGB;
  case TextureFormat::BGRA_SRGB:
    return MTLPixelFormatBGRA8Unorm_sRGB;
  case TextureFormat::RGBA_F16:
    return MTLPixelFormatRGBA16Float;
  case TextureFormat::RG_F16:
    return MTLPixelFormatRG16Float;
  case TextureFormat::RG_UInt16:
    return MTLPixelFormatRG16Uint;
  case TextureFormat::RG_UNorm16:
    return MTLPixelFormatRG16Unorm;

  case TextureFormat::RGB10_A2_UNorm_Rev:
    return MTLPixelFormatRGB10A2Unorm;
  case TextureFormat::RGB10_A2_Uint_Rev:
    return MTLPixelFormatRGB10A2Uint;
  case TextureFormat::BGR10_A2_Unorm:
    return MTLPixelFormatBGR10A2Unorm;

  case TextureFormat::R_F32:
    return MTLPixelFormatR32Float;

  // 96 bit
  case TextureFormat::RGB_F32:
    return MTLPixelFormatInvalid;
  // 128 bps
  case TextureFormat::RGBA_UInt32:
    return MTLPixelFormatRGBA32Uint;
  case TextureFormat::RGBA_F32:
    return MTLPixelFormatRGBA32Float;
  case TextureFormat::LA_UNorm8:
    // This format should not be used in Metal. Use RG_UNorm8 instead.

  case TextureFormat::BGRA_UNorm8_Rev:
    // On the OGL side, this format is used to convert BGRA image to RGBA
    // during upload. Not sure if we can do this with Metal
  case TextureFormat::L_UNorm8: // No Metal equivalent
  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
  case TextureFormat::RGB_F16:
  case TextureFormat::RGBX_UNorm8:
  case TextureFormat::R5G5B5A1_UNorm:
    return MTLPixelFormatInvalid;

    // Compressed
  case TextureFormat::RGBA_ASTC_4x4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_4x4_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_4x4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_4x4_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_5x4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x4_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_5x4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x4_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_5x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_5x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_6x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_6x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_6x6:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x6_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_6x6:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x6_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_8x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_8x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_8x6:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x6_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_8x6:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x6_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_8x8:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x8_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_8x8:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x8_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x5:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x6:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x6_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x6:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x6_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x8:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x8_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x8:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x8_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x10:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x10_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x10:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x10_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_12x10:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x10_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_12x10:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x10_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_12x12:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x12_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_12x12:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x12_sRGB;
#endif

  case TextureFormat::RGBA_PVRTC_2BPPV1:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGBA_2BPP;
#endif

  case TextureFormat::RGB_PVRTC_2BPPV1:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGB_2BPP;
#endif

  case TextureFormat::RGBA_PVRTC_4BPPV1:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGBA_4BPP;
#endif

  case TextureFormat::RGB_PVRTC_4BPPV1:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGB_4BPP;
#endif

  case TextureFormat::RGB8_ETC1:
  case TextureFormat::RGB8_ETC2:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatETC2_RGB8;
#endif

  case TextureFormat::RGBA8_EAC_ETC2:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RGBA8;
#endif

  case TextureFormat::SRGB8_ETC2:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatETC2_RGB8_sRGB;
#endif

  case TextureFormat::SRGB8_A8_EAC_ETC2:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RGBA8_sRGB;
#endif

  case TextureFormat::RG_EAC_UNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RG11Unorm;
#endif

  case TextureFormat::RG_EAC_SNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RG11Snorm;
#endif

  case TextureFormat::R_EAC_UNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_R11Unorm;
#endif

  case TextureFormat::R_EAC_SNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_R11Snorm;
#endif

    // MTLPixelFormatBC7_RGBAUnorm supported only on MacOS and Catalyst
    // https://developer.apple.com/documentation/metal/mtlpixelformat/mtlpixelformatbc7_rgbaunorm
  case TextureFormat::RGBA_BC7_UNORM_4x4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatBC7_RGBAUnorm;
#else
    return MTLPixelFormatInvalid;
#endif

    // Depth & Stencil
  case TextureFormat::Z_UNorm16:
    return MTLPixelFormatDepth32Float;
  case TextureFormat::Z_UNorm24:
    return MTLPixelFormatDepth32Float;
  case TextureFormat::Z_UNorm32:
    return MTLPixelFormatDepth32Float;
  case TextureFormat::S8_UInt_Z24_UNorm:
    return MTLPixelFormatDepth32Float_Stencil8;
  case TextureFormat::S8_UInt_Z32_UNorm:
    return MTLPixelFormatDepth32Float_Stencil8;
  case TextureFormat::S_UInt8:
    return MTLPixelFormatStencil8;
  }
}

TextureFormat Texture::mtlPixelFormatToTextureFormat(MTLPixelFormat value) {
// Some fbsource targets don't allow enum default, this makes sure all of them compile
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
  switch (value) {
  // 8 bpp
  case MTLPixelFormatR8Unorm:
    return TextureFormat::R_UNorm8;
  case MTLPixelFormatA8Unorm:
    return TextureFormat::A_UNorm8;

// 16 bpp
#if !(IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST) // No support for these on macOS
  case MTLPixelFormatBGR5A1Unorm:
    return TextureFormat::B5G5R5A1_UNorm;
  case MTLPixelFormatB5G6R5Unorm:
    return TextureFormat::B5G6R5_UNorm;
  case MTLPixelFormatABGR4Unorm:
    return TextureFormat::ABGR_UNorm4;
#endif
  case MTLPixelFormatRG8Unorm:
    return TextureFormat::RG_UNorm8;
  case MTLPixelFormatBGRG422:
    return TextureFormat::R4G2B2_UNorm_Apple;
  case MTLPixelFormatGBGR422:
    return TextureFormat::R4G2B2_UNorm_Rev_Apple;
  case MTLPixelFormatR16Uint:
    return TextureFormat::R_UInt16;
  case MTLPixelFormatR16Unorm:
    return TextureFormat::R_UNorm16;

    // 32 bpp
  case MTLPixelFormatRGBA8Unorm:
    return TextureFormat::RGBA_UNorm8;
  case MTLPixelFormatBGRA8Unorm:
    return TextureFormat::BGRA_UNorm8;
  case MTLPixelFormatBGRA8Unorm_sRGB:
    return TextureFormat::BGRA_SRGB;

  case MTLPixelFormatRGBA8Unorm_sRGB:
    return TextureFormat::RGBA_SRGB;

  case MTLPixelFormatRGB10A2Unorm:
    return TextureFormat::RGB10_A2_UNorm_Rev;
  case MTLPixelFormatRGB10A2Uint:
    return TextureFormat::RGB10_A2_Uint_Rev;
  case MTLPixelFormatBGR10A2Unorm:
    return TextureFormat::BGR10_A2_Unorm;

  case MTLPixelFormatRGBA16Float:
    return TextureFormat::RGBA_F16;
  case MTLPixelFormatR16Float:
    return TextureFormat::R_F16;
  case MTLPixelFormatRG16Float:
    return TextureFormat::RG_F16;
  case MTLPixelFormatRG16Uint:
    return TextureFormat::RG_UInt16;
  case MTLPixelFormatRG16Unorm:
    return TextureFormat::RG_UNorm16;

  case MTLPixelFormatR32Float:
    return TextureFormat::R_F32;

  case MTLPixelFormatRGBA32Uint:
    return TextureFormat::RGBA_UInt32;
  case MTLPixelFormatRGBA32Float:
    return TextureFormat::RGBA_F32;

// Compressed
#if !(IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST)
  case MTLPixelFormatASTC_4x4_LDR:
    return TextureFormat::RGBA_ASTC_4x4;
  case MTLPixelFormatASTC_4x4_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_4x4;
  case MTLPixelFormatASTC_5x4_LDR:
    return TextureFormat::RGBA_ASTC_5x4;
  case MTLPixelFormatASTC_5x4_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_5x4;
  case MTLPixelFormatASTC_5x5_LDR:
    return TextureFormat::RGBA_ASTC_5x5;
  case MTLPixelFormatASTC_5x5_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_5x5;
  case MTLPixelFormatASTC_6x5_LDR:
    return TextureFormat::RGBA_ASTC_6x5;
  case MTLPixelFormatASTC_6x5_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_6x5;
  case MTLPixelFormatASTC_6x6_LDR:
    return TextureFormat::RGBA_ASTC_6x6;
  case MTLPixelFormatASTC_6x6_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_6x6;
  case MTLPixelFormatASTC_8x5_LDR:
    return TextureFormat::RGBA_ASTC_8x5;
  case MTLPixelFormatASTC_8x5_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_8x5;
  case MTLPixelFormatASTC_8x6_LDR:
    return TextureFormat::RGBA_ASTC_8x6;
  case MTLPixelFormatASTC_8x6_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_8x6;
  case MTLPixelFormatASTC_8x8_LDR:
    return TextureFormat::RGBA_ASTC_8x8;
  case MTLPixelFormatASTC_8x8_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_8x8;
  case MTLPixelFormatASTC_10x5_LDR:
    return TextureFormat::RGBA_ASTC_10x5;
  case MTLPixelFormatASTC_10x5_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_10x5;
  case MTLPixelFormatASTC_10x6_LDR:
    return TextureFormat::RGBA_ASTC_10x6;
  case MTLPixelFormatASTC_10x6_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_10x6;
  case MTLPixelFormatASTC_10x8_LDR:
    return TextureFormat::RGBA_ASTC_10x8;
  case MTLPixelFormatASTC_10x8_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_10x8;
  case MTLPixelFormatASTC_10x10_LDR:
    return TextureFormat::RGBA_ASTC_10x10;
  case MTLPixelFormatASTC_10x10_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_10x10;
  case MTLPixelFormatASTC_12x10_LDR:
    return TextureFormat::RGBA_ASTC_12x10;
  case MTLPixelFormatASTC_12x10_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_12x10;
  case MTLPixelFormatASTC_12x12_LDR:
    return TextureFormat::RGBA_ASTC_12x12;
  case MTLPixelFormatASTC_12x12_sRGB:
    return TextureFormat::SRGB8_A8_ASTC_12x12;
  case MTLPixelFormatPVRTC_RGBA_2BPP:
    return TextureFormat::RGBA_PVRTC_2BPPV1;
  case MTLPixelFormatPVRTC_RGB_2BPP:
    return TextureFormat::RGB_PVRTC_2BPPV1;
  case MTLPixelFormatPVRTC_RGBA_4BPP:
    return TextureFormat::RGBA_PVRTC_4BPPV1;
  case MTLPixelFormatPVRTC_RGB_4BPP:
    return TextureFormat::RGB_PVRTC_4BPPV1;
  case MTLPixelFormatETC2_RGB8:
    return TextureFormat::RGB8_ETC2;
  case MTLPixelFormatEAC_RGBA8:
    return TextureFormat::RGBA8_EAC_ETC2;
#endif
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
  case MTLPixelFormatBC7_RGBAUnorm:
    return TextureFormat::RGBA_BC7_UNORM_4x4;
#endif

    // Depth & Stencil
  case MTLPixelFormatDepth32Float:
    return TextureFormat::Z_UNorm32;
  case MTLPixelFormatDepth32Float_Stencil8:
    return TextureFormat::S8_UInt_Z32_UNorm;
  case MTLPixelFormatStencil8:
    return TextureFormat::S_UInt8;

  default:
    return TextureFormat::Invalid;
  }

#pragma clang diagnostic pop
}

} // namespace metal
} // namespace igl
