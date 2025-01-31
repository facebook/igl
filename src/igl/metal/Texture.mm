/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/Texture.h>

#include <igl/metal/CommandBuffer.h>
#include <vector>

namespace {

void bgrToRgb(unsigned char* dstImg, size_t width, size_t height, size_t bytesPerPixel) {
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      auto pixelIndex = i * width + j;
      std::swap(dstImg[pixelIndex * bytesPerPixel + 0], dstImg[pixelIndex * bytesPerPixel + 2]);
    }
  }
}
} // namespace

namespace igl::metal {

Texture::Texture(id<MTLTexture> texture, const ICapabilities& capabilities) :
  ITexture(mtlPixelFormatToTextureFormat([texture pixelFormat])),
  value_(texture),
  drawable_(nullptr),
  capabilities_(capabilities) {}

Texture::Texture(id<CAMetalDrawable> drawable, const ICapabilities& capabilities) :
  ITexture(mtlPixelFormatToTextureFormat([drawable.texture pixelFormat])),
  value_(nullptr),
  drawable_(drawable),
  capabilities_(capabilities) {}

Texture::~Texture() {
  value_ = nil;
}

bool Texture::needsRepacking(const TextureRangeDesc& range, size_t bytesPerRow) const {
  if (bytesPerRow == 0) {
    return false;
  }

  const auto format = getFormat();
  if (format == TextureFormat::RGBA_PVRTC_2BPPV1 || format == TextureFormat::RGB_PVRTC_2BPPV1 ||
      format == TextureFormat::RGBA_PVRTC_4BPPV1 || format == TextureFormat::RGB_PVRTC_4BPPV1) {
    // PVRTC formats MUST pass a value of 0 to bytesPerRow, so MUST be repacked if bytesPerRow
    // doesn't match the range's bytesPerRow.
    const auto rangeBytesPerRow = getProperties().getBytesPerRow(range);
    return rangeBytesPerRow != bytesPerRow;
  }

  // Metal textures MUST be aligned to a multiple of the texel size or, for compressed textures, the
  // texel block size.
  return bytesPerRow % getProperties().bytesPerBlock != 0;
}

Result Texture::uploadInternal(TextureType type,
                               const TextureRangeDesc& range,
                               const void* IGL_NULLABLE data,
                               size_t bytesPerRow) const {
  if (data == nullptr) {
    return Result(Result::Code::Ok);
  }
  const auto& properties = getProperties();
  const auto rangeBytesPerRow = bytesPerRow > 0 ? bytesPerRow : properties.getBytesPerRow(range);

  const auto initialSlice = getMetalSlice(type, range.face, range.layer);
  const auto numSlices = getMetalSlice(type, range.numFaces, range.numLayers);
  for (auto mipLevel = range.mipLevel; mipLevel < range.mipLevel + range.numMipLevels; ++mipLevel) {
    const auto mipRange = range.atMipLevel(mipLevel);
    for (auto slice = initialSlice; slice < initialSlice + numSlices; ++slice) {
      const auto sliceRange = atMetalSlice(type, mipRange, slice);
      const auto* sliceData = getSubRangeStart(data, range, sliceRange, bytesPerRow);
      const auto region = MTLRegionMake3D(sliceRange.x,
                                          sliceRange.y,
                                          sliceRange.z,
                                          sliceRange.width,
                                          sliceRange.height,
                                          sliceRange.depth);
      switch (type) {
      case TextureType::Cube:
      case TextureType::TwoD:
      case TextureType::TwoDArray: {
        [get() replaceRegion:region
                 mipmapLevel:sliceRange.mipLevel
                       slice:getMetalSlice(type, sliceRange.face, sliceRange.layer)
                   withBytes:sliceData
                 bytesPerRow:toMetalBytesPerRow(rangeBytesPerRow)
               bytesPerImage:0];
        break;
      }
      case TextureType::ThreeD: {
        [get() replaceRegion:region
                 mipmapLevel:sliceRange.mipLevel
                       slice:0 /* 3D array textures not supported */
                   withBytes:sliceData
                 bytesPerRow:toMetalBytesPerRow(rangeBytesPerRow)
               bytesPerImage:toMetalBytesPerRow(rangeBytesPerRow * sliceRange.height)];
        break;
      }
      default:
        IGL_DEBUG_ABORT("Unknown texture type");
        break;
      }
    }
  }
  return Result{};
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
  if (range.numLayers > 1 || range.numFaces > 1) {
    return Result(Result::Code::Unsupported,
                  "Can't retrieve data from more than one face or layer");
  }
  const auto& properties = getProperties();
  if (bytesPerRow == 0) {
    bytesPerRow = properties.getBytesPerRow(range);
  }

  const size_t bytesPerImage = properties.getBytesPerRange(range);
  const MTLRegion region = {{range.x, range.y, 0}, {range.width, range.height, 1}};
  auto tmpBuffer = std::make_unique<uint8_t[]>(bytesPerImage);

  [get() getBytes:tmpBuffer.get()
        bytesPerRow:toMetalBytesPerRow(properties.getBytesPerRow(range))
      bytesPerImage:bytesPerImage
         fromRegion:region
        mipmapLevel:range.mipLevel
              slice:getMetalSlice(getType(), range.face, range.layer)];

  /// Metal textures are up-side down compared to OGL textures. IGL follows
  /// the OGL convention and this function flips the texture vertically
  repackData(
      properties, range, tmpBuffer.get(), 0, static_cast<uint8_t*>(outData), bytesPerRow, true);

  const igl::TextureFormat f = getFormat();
  const TextureFormatProperties props = TextureFormatProperties::fromTextureFormat(f);
  auto bytesPerPixel = props.bytesPerBlock;
  if (f == TextureFormat::BGRA_SRGB || f == TextureFormat::BGRA_UNorm8) {
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

Dimensions Texture::getDimensions() const {
  auto texture = get();
  return Dimensions{static_cast<uint32_t>([texture width]),
                    static_cast<uint32_t>([texture height]),
                    static_cast<uint32_t>([texture depth])};
}

uint32_t Texture::getNumLayers() const {
  return [get() arrayLength];
}

TextureType Texture::getType() const {
  return convertType([get() textureType]);
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return toTextureUsage([get() usage]);
}

uint32_t Texture::getSamples() const {
  return [get() sampleCount];
}

uint32_t Texture::getNumMipLevels() const {
  return [get() mipmapLevelCount];
}

void Texture::generateMipmap(ICommandQueue& cmdQueue, const TextureRangeDesc* range) const {
  if (range) {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  }

  if (value_.mipmapLevelCount > 1) {
    auto mtlCmdQueue = static_cast<CommandQueue&>(cmdQueue).get();

    const id<MTLCommandBuffer> mtlCmdBuffer = [mtlCmdQueue commandBuffer];
    if (mtlCmdBuffer) {
      generateMipmap(mtlCmdBuffer);
      [mtlCmdBuffer commit];
    }
  }
}

void Texture::generateMipmap(ICommandBuffer& cmdBuffer, const TextureRangeDesc* range) const {
  if (range) {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  }

  if (value_.mipmapLevelCount > 1) {
    auto mtlCmdBuffer = static_cast<CommandBuffer&>(cmdBuffer).get();
    generateMipmap(mtlCmdBuffer);
  }
}

void Texture::generateMipmap(id<MTLCommandBuffer> cmdBuffer) const {
  // we can only generate mipmaps for filterable texture formats via the blit encoder
  const bool isFilterable = (capabilities_.getTextureFormatCapabilities(getFormat()) &
                             ICapabilities::TextureFormatCapabilityBits::SampledFiltered) != 0;
  if (!isFilterable) {
    // TODO: implement manual mip generation for required formats (e.g. RGBA32Float)
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return;
  }

  const id<MTLBlitCommandEncoder> encoder = [cmdBuffer blitCommandEncoder];
  [encoder generateMipmapsForTexture:value_];
  [encoder endEncoding];
}

bool Texture::isRequiredGenerateMipmap() const {
  return value_.mipmapLevelCount > 1;
}

uint64_t Texture::getTextureId() const {
  // TODO: implement via gpuResourceID
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  return 0;
}

TextureDesc::TextureUsage Texture::toTextureUsage(MTLTextureUsage usage) {
  TextureDesc::TextureUsage result = 0;
  result |= ((usage & MTLTextureUsageShaderRead) != 0) ? TextureDesc::TextureUsageBits::Sampled : 0;
  result |= ((usage & MTLTextureUsageShaderWrite) != 0) ? TextureDesc::TextureUsageBits::Storage
                                                        : 0;
  result |= ((usage & MTLTextureUsageRenderTarget) != 0) ? TextureDesc::TextureUsageBits::Attachment
                                                         : 0;
  return result;
}

MTLTextureUsage Texture::toMTLTextureUsage(TextureDesc::TextureUsage usage) {
  MTLTextureUsage result = 0;
  result |= ((usage & TextureDesc::TextureUsageBits::Sampled) != 0) ? MTLTextureUsageShaderRead : 0;
  result |= ((usage & TextureDesc::TextureUsageBits::Storage) != 0) ? MTLTextureUsageShaderWrite
                                                                    : 0;
  result |= ((usage & TextureDesc::TextureUsageBits::Attachment) != 0) ? MTLTextureUsageRenderTarget
                                                                       : 0;
  return result;
}

MTLTextureType Texture::convertType(TextureType value, size_t numSamples) {
  IGL_DEBUG_ASSERT(value != TextureType::Invalid && value != TextureType::ExternalImage);

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
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatBGR5A1Unorm;
#endif
  case TextureFormat::B5G6R5_UNorm:
#if IGL_PLATFORM_MACOSX || IGL_PLATFORM_IOS_SIMULATOR
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatB5G6R5Unorm;
#endif
  case TextureFormat::ABGR_UNorm4:
#if IGL_PLATFORM_MACOSX || IGL_PLATFORM_IOS_SIMULATOR
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

  case TextureFormat::R_UInt32:
    return MTLPixelFormatR32Uint;

  case TextureFormat::RG_F32:
    return MTLPixelFormatRG32Float;

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
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_4x4_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_4x4:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_4x4_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_5x4:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x4_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_5x4:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x4_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_5x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_5x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_5x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_6x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_6x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_6x6:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x6_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_6x6:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_6x6_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_8x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_8x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_8x6:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x6_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_8x6:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x6_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_8x8:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x8_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_8x8:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_8x8_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x5_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x5:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x5_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x6:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x6_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x6:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x6_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x8:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x8_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x8:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x8_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_10x10:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x10_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_10x10:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_10x10_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_12x10:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x10_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_12x10:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x10_sRGB;
#endif

  case TextureFormat::RGBA_ASTC_12x12:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x12_LDR;
#endif

  case TextureFormat::SRGB8_A8_ASTC_12x12:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatASTC_12x12_sRGB;
#endif

  case TextureFormat::RGBA_PVRTC_2BPPV1:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGBA_2BPP;
#endif

  case TextureFormat::RGB_PVRTC_2BPPV1:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGB_2BPP;
#endif

  case TextureFormat::RGBA_PVRTC_4BPPV1:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGBA_4BPP;
#endif

  case TextureFormat::RGB_PVRTC_4BPPV1:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatPVRTC_RGB_4BPP;
#endif

  case TextureFormat::RGB8_ETC1:
  case TextureFormat::RGB8_ETC2:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatETC2_RGB8;
#endif

  case TextureFormat::RGBA8_EAC_ETC2:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RGBA8;
#endif

  case TextureFormat::SRGB8_ETC2:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatETC2_RGB8_sRGB;
#endif

  case TextureFormat::SRGB8_A8_EAC_ETC2:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RGBA8_sRGB;
#endif

  case TextureFormat::RG_EAC_UNorm:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RG11Unorm;
#endif

  case TextureFormat::RG_EAC_SNorm:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_RG11Snorm;
#endif

  case TextureFormat::R_EAC_UNorm:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_R11Unorm;
#endif

  case TextureFormat::R_EAC_SNorm:
#if IGL_PLATFORM_MACOSX
    return MTLPixelFormatInvalid;
#else
    return MTLPixelFormatEAC_R11Snorm;
#endif

    // MTLPixelFormatBC7_RGBAUnorm supported only on MacOS and Catalyst
    // https://developer.apple.com/documentation/metal/mtlpixelformat/mtlpixelformatbc7_rgbaunorm
  case TextureFormat::RGBA_BC7_UNORM_4x4:
#if IGL_PLATFORM_MACOSX || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatBC7_RGBAUnorm;
#else
    return MTLPixelFormatInvalid;
#endif
  case TextureFormat::RGBA_BC7_SRGB_4x4:
#if IGL_PLATFORM_MACOSX || IGL_PLATFORM_MACCATALYST
    return MTLPixelFormatBC7_RGBAUnorm_sRGB;
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

  case TextureFormat::YUV_NV12:
  case TextureFormat::YUV_420p:
    return MTLPixelFormatInvalid;
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
#if !IGL_PLATFORM_MACOSX // No support for these on macOS
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
  case MTLPixelFormatR32Uint:
    return TextureFormat::R_UInt32;
  case MTLPixelFormatRG32Float:
    return TextureFormat::RG_F32;
  case MTLPixelFormatRGBA32Uint:
    return TextureFormat::RGBA_UInt32;
  case MTLPixelFormatRGBA32Float:
    return TextureFormat::RGBA_F32;

// Compressed
#if !IGL_PLATFORM_MACOSX
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
#if IGL_PLATFORM_MACOSX
  case MTLPixelFormatBC7_RGBAUnorm:
    return TextureFormat::RGBA_BC7_UNORM_4x4;
  case MTLPixelFormatBC7_RGBAUnorm_sRGB:
    return TextureFormat::RGBA_BC7_SRGB_4x4;
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

NSUInteger Texture::getMetalSlice(TextureType type, uint32_t face, uint32_t layer) {
  return type == TextureType::Cube ? face : layer;
}

TextureRangeDesc Texture::atMetalSlice(TextureType type,
                                       const TextureRangeDesc& range,
                                       NSUInteger metalSlice) {
  return type == TextureType::Cube ? range.atFace(static_cast<uint32_t>(metalSlice))
                                   : range.atLayer(static_cast<uint32_t>(metalSlice));
}

} // namespace igl::metal
