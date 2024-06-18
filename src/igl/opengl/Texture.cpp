/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Texture.h>

#include <algorithm>
#include <cstdlib>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/util/TextureFormat.h>

namespace igl {
namespace opengl {

Dimensions Texture::getDimensions() const {
  return Dimensions{
      static_cast<size_t>(width_), static_cast<size_t>(height_), static_cast<size_t>(depth_)};
}

size_t Texture::getNumLayers() const {
  return numLayers_;
}

uint32_t Texture::getSamples() const {
  return numSamples_;
}

void Texture::generateMipmap(ICommandQueue& /* unused */,
                             const TextureRangeDesc* IGL_NULLABLE /* unused */) const {
  IGL_ASSERT_MSG(0, "Can only generate mipmap for R/W texture (eg. TextureBuffer).");
}

void Texture::generateMipmap(ICommandBuffer& /* unused */,
                             const TextureRangeDesc* IGL_NULLABLE /* unused */) const {
  IGL_ASSERT_MSG(0, "Can only generate mipmap for R/W texture (eg. TextureBuffer).");
}

uint32_t Texture::getNumMipLevels() const {
  return numMipLevels_;
}

bool Texture::isRequiredGenerateMipmap() const {
  return false;
}

uint64_t Texture::getTextureId() const {
  // this requires ARB_bindless_texture
  IGL_ASSERT_NOT_IMPLEMENTED();
  return 0;
}

bool Texture::isSwapchainTexture() const {
  return isImplicitStorage();
}

Result Texture::create(const TextureDesc& desc, bool hasStorageAlready) {
  Result result;
  if (desc.numLayers > 1 && desc.type != TextureType::TwoDArray) {
    return Result{Result::Code::Unsupported,
                  "Array textures are only supported when type is TwoDArray."};
  }
  if (IGL_VERIFY(!isCreated_)) {
    isCreated_ = true;
    IGL_ASSERT(desc.format != TextureFormat::Invalid && desc.format == getFormat());
    const bool isSampled = (desc.usage & TextureDesc::TextureUsageBits::Sampled) != 0;

    if (isSampled && hasStorageAlready) {
      result = Result(Result::Code::Unsupported,
                      "TextureUsageBits::Sampled and hasStorageAlready unsupported on GLES (we "
                      "can't read from an EAGLLayer backed renderbuffer)");
    }

    width_ = (GLsizei)desc.width;
    height_ = (GLsizei)desc.height;
    depth_ = desc.depth;
    type_ = desc.type;
    numLayers_ = desc.numLayers;
    numSamples_ = desc.numSamples;
    numMipLevels_ = desc.numMipLevels;
    if (!getContext().deviceFeatures().hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // For ES 2.0, we have to ignore numMipLevels_
      const auto maxNumMipLevels = TextureDesc::calcNumMipLevels(width_, height_, depth_);
      if (numMipLevels_ > 1 && numMipLevels_ != maxNumMipLevels) {
        IGL_LOG_ERROR("Partial mip chains are not supported so numMipLevels_ will be set to %d",
                      maxNumMipLevels);
        numMipLevels_ = maxNumMipLevels;
      }
    }
  } else {
    result = Result(Result::Code::InvalidOperation, "Texture already created");
  }
  return result;
}

// Gets pack/unpack alignment for pixelStorei
// stride is the number of bytes for a row of the image (image width * bytes per pixel)+padding
//
// openGL only uses alignment instead of stride when reading/writing pixels so it will not support
// padding that is not 8, 4, 2, or 1 byte aligned to the actual pixel data

GLint Texture::getAlignment(size_t stride, size_t mipLevel) const {
  IGL_ASSERT(mipLevel < numMipLevels_);

  if (getProperties().isCompressed()) {
    return 1;
  }

  // Clamp to 1 to account for non-square textures.
  const auto srcWidth = std::max(getDimensions().width >> mipLevel, static_cast<size_t>(1));

  const auto pixelBytesPerRow = getProperties().getBytesPerRow(srcWidth);

  if (stride == 0 || !IGL_VERIFY(pixelBytesPerRow <= stride)) {
    return 1;
  } else if (stride % 8 == 0) {
    return 8;
  } else if (stride % 4 == 0) {
    return 4;
  } else if (stride % 2 == 0) {
    return 2;
  } else {
    return 1;
  }
}

bool Texture::isImplicitStorage() const {
  return false;
}

GLenum Texture::toGLTarget(TextureType type) const {
  switch (type) {
  case TextureType::TwoD:
    return GL_TEXTURE_2D;
  case TextureType::TwoDArray:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::Texture2DArray)) {
      return GL_TEXTURE_2D_ARRAY;
    }
    break;
  case TextureType::ThreeD:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::Texture3D)) {
      return GL_TEXTURE_3D;
    }
    break;
  case TextureType::Cube:
    return GL_TEXTURE_CUBE_MAP;
  case TextureType::ExternalImage:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureExternalImage)) {
      return GL_TEXTURE_EXTERNAL_OES;
    }
    break;
  case TextureType::Invalid:
    break;
  }
  IGL_ASSERT_MSG(0, "Unsupported OGL Texture Type: %d", type);

  return 0;
}

// Whenever possible the caller should have the incoming format in
// igl::TextureFormat thus not use this function. For the cases when this
// is not possible, e.g. dictated by a file header, then this function
// can convert GL Texture format into IGL Texture Format
// This method assumes no swizzling is required (eg. GL_RED results in R_UNorm8 but it could be
// A_UNorm8 with swizzling)
TextureFormat Texture::glInternalFormatToTextureFormat(GLuint glTexInternalFormat,
                                                       GLuint glTexFormat,
                                                       GLuint glTexType) {
  return util::glTextureFormatToTextureFormat(static_cast<int32_t>(glTexInternalFormat),
                                              static_cast<uint32_t>(glTexFormat),
                                              static_cast<uint32_t>(glTexType));
}
bool Texture::toFormatDescGL(TextureFormat textureFormat,
                             TextureDesc::TextureUsage usage,
                             FormatDescGL& outFormatGL) const {
  return toFormatDescGL(getContext(), textureFormat, usage, outFormatGL);
}
bool Texture::toFormatDescGL(IContext& ctx,
                             TextureFormat textureFormat,
                             TextureDesc::TextureUsage usage,
                             FormatDescGL& outFormatGL) {
  const auto& deviceFeatures = ctx.deviceFeatures();

  // TODO: Remove these fallbacks once devices can properly provide a supported format
  if (textureFormat == TextureFormat::S8_UInt_Z32_UNorm &&
      !deviceFeatures.hasTextureFeature(TextureFeatures::Depth32FStencil8)) {
    textureFormat = TextureFormat::S8_UInt_Z24_UNorm;
  }
  if (textureFormat == TextureFormat::Z_UNorm24) {
    if ((usage & TextureDesc::TextureUsageBits::Sampled) != 0 &&
        !deviceFeatures.hasTextureFeature(TextureFeatures::DepthTexImage24)) {
      textureFormat = TextureFormat::Z_UNorm32;
    }
    if ((usage & TextureDesc::TextureUsageBits::Attachment) != 0 &&
        !deviceFeatures.hasTextureFeature(TextureFeatures::DepthRenderbuffer24)) {
      textureFormat = TextureFormat::Z_UNorm32;
    }
    if ((usage & TextureDesc::TextureUsageBits::Storage) != 0 &&
        !deviceFeatures.hasTextureFeature(TextureFeatures::DepthTexStorage24)) {
      textureFormat = TextureFormat::Z_UNorm32;
    }
  }

  bool sampled = (usage & TextureDesc::TextureUsageBits::Sampled) != 0;
  bool attachment = (usage & TextureDesc::TextureUsageBits::Attachment) != 0;
  bool storage = (usage & TextureDesc::TextureUsageBits::Storage) != 0;
  bool sampledAttachment = sampled && attachment;
  bool sampledOnly = sampled && !attachment;
  bool attachmentOnly = attachment && !sampled;

  // Sanity check capabilities
  auto capabilities = deviceFeatures.getTextureFormatCapabilities(textureFormat);
  // Fallback for Z_UNorm32, some devices capabilities do not support this format,
  // usually Z_UNorm24 would suffice.
  if (capabilities == 0 && (textureFormat == TextureFormat::Z_UNorm32)) {
    IGL_LOG_INFO("Device does not support 32-bit depth format (%s). Falling back to 24-bit\n",
                 TextureFormatProperties::fromTextureFormat(textureFormat).name);
    textureFormat = TextureFormat::Z_UNorm24;
    capabilities = deviceFeatures.getTextureFormatCapabilities(textureFormat);
    if (capabilities == 0) {
      IGL_LOG_INFO("Device does not support 24-bit depth format (%s). Falling back to 16-bit\n",
                   TextureFormatProperties::fromTextureFormat(textureFormat).name);
      textureFormat = TextureFormat::Z_UNorm16;
      capabilities = deviceFeatures.getTextureFormatCapabilities(TextureFormat::Z_UNorm16);
      if (capabilities == 0) {
        IGL_LOG_ERROR("Device does not support basic 16-bit depth format (%s). Erroring out\n",
                      TextureFormatProperties::fromTextureFormat(textureFormat).name);
        return false;
      }
    }
  }

  if (attachmentOnly &&
      (capabilities & ICapabilities::TextureFormatCapabilityBits::Attachment) == 0) {
    IGL_LOG_ERROR("Texture format %s does not support Attachment usage.\n",
                  TextureFormatProperties::fromTextureFormat(textureFormat).name);
    return false;
  }
  if (sampledOnly && (capabilities & ICapabilities::TextureFormatCapabilityBits::Sampled) == 0) {
    IGL_LOG_ERROR("Texture format %s does not support Sampled usage.\n",
                  TextureFormatProperties::fromTextureFormat(textureFormat).name);
    return false;
  }
  if (storage && (capabilities & ICapabilities::TextureFormatCapabilityBits::Storage) == 0) {
    IGL_LOG_ERROR("Texture format %s does not support Storage usage.\n",
                  TextureFormatProperties::fromTextureFormat(textureFormat).name);
    return false;
  }
  if (sampledAttachment &&
      (capabilities & ICapabilities::TextureFormatCapabilityBits::SampledAttachment) == 0) {
    if ((capabilities & ICapabilities::TextureFormatCapabilityBits::Sampled) != 0) {
      IGL_LOG_INFO(
          "Texture format %s does not support SampledAttachment usage. Falling back to Sampled.\n",
          TextureFormatProperties::fromTextureFormat(textureFormat).name);
      sampledOnly = true;
      sampledAttachment = false;
      attachmentOnly = false;
      attachment = false;
    } else {
      IGL_LOG_ERROR("Texture format %s does not support SampledAttachment usage.\n",
                    TextureFormatProperties::fromTextureFormat(textureFormat).name);
      return false;
    }
  }

  // Uncompressed textures can request RenderbufferStorage, TexStorage or TexImage.
  // TexStorage takes precedence over TexImage if it is requested.

  const bool renderbuffer = attachmentOnly;
  const bool texStorage = storage;
  const bool texImage = !storage && sampled;
  if (!renderbuffer && !texStorage && !texImage) {
    return false;
  }

  // Compressed textures formats can be used if either TexStorage or TexImage is requested.
  const bool compressedTexStorage =
      storage && deviceFeatures.hasTextureFeature(TextureFeatures::TextureCompressionTexStorage);
  const bool compressedTexImage = sampled;
  const bool compressedValid = compressedTexStorage || compressedTexImage;

  auto& format = outFormatGL.format;
  auto& type = outFormatGL.type;
  auto& internalFormat = outFormatGL.internalFormat;

  switch (textureFormat) {
  case TextureFormat::Invalid:
    return false;

  case TextureFormat::RGBA_UNorm8:
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_RGBA8;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageRgba8)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::RGBA_SRGB:
    format = deviceFeatures.hasExtension(Extensions::Srgb) ? GL_SRGB_ALPHA : GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_SRGB8_ALPHA8;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageSrgba8)) {
      internalFormat = GL_SRGB_ALPHA;
    }
    return true;

  case TextureFormat::BGRA_SRGB:
    format = GL_BGRA;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_SRGB8_ALPHA8;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageSrgba8)) {
      internalFormat = GL_SRGB_ALPHA;
    }
    return true;

  case TextureFormat::R4G2B2_UNorm_Apple:
    format = GL_RGB_422_APPLE;
    type = GL_UNSIGNED_SHORT_8_8_APPLE;
    internalFormat = GL_RGB_RAW_422_APPLE;
    if (texImage && deviceFeatures.hasInternalRequirement(
                        InternalRequirement::ColorTexImageRgbApple422Unsized)) {
      internalFormat = GL_RGB;
    }
    return true;

  case TextureFormat::R4G2B2_UNorm_Rev_Apple:
    format = GL_RGB_422_APPLE;
    type = GL_UNSIGNED_SHORT_8_8_REV_APPLE;
    internalFormat = GL_RGB_RAW_422_APPLE;
    if (texImage && deviceFeatures.hasInternalRequirement(
                        InternalRequirement::ColorTexImageRgbApple422Unsized)) {
      internalFormat = GL_RGB;
    }
    return true;

  case TextureFormat::R5G5B5A1_UNorm:
    format = GL_RGBA;
    type = GL_UNSIGNED_SHORT_5_5_5_1;
    internalFormat = GL_RGB5_A1;
    if (texImage &&
        deviceFeatures.hasInternalRequirement(InternalRequirement::ColorTexImageRgb5A1Unsized)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::RGBX_UNorm8:
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_RGB8;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageRgba8)) {
      internalFormat = GL_RGB;
    }
    return true;

  case TextureFormat::RGBA_F32:
    format = GL_RGBA;
    type = GL_FLOAT;
    internalFormat = GL_RGBA32F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage32f)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::RGB_F32:
    format = GL_RGB;
    type = GL_FLOAT;
    internalFormat = GL_RGB32F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage32f)) {
      internalFormat = GL_RGB;
    }
    return true;

  case TextureFormat::RGBA_F16:
    format = GL_RGBA;
    if (deviceFeatures.hasInternalRequirement(InternalRequirement::TextureHalfFloatExtReq)) {
      type = GL_HALF_FLOAT_OES; // NOTE: NOT the same as GL_HALF_FLOAT
    } else {
      type = GL_HALF_FLOAT;
    }
    internalFormat = GL_RGBA16F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage16f)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::RGB_F16:
    format = GL_RGB;
    if (deviceFeatures.hasInternalRequirement(InternalRequirement::TextureHalfFloatExtReq)) {
      type = GL_HALF_FLOAT_OES; // NOTE: NOT the same as GL_HALF_FLOAT
    } else {
      type = GL_HALF_FLOAT;
    }
    internalFormat = GL_RGB16F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage16f)) {
      internalFormat = GL_RGB;
    }
    return true;

  case TextureFormat::BGRA_UNorm8:
    format = GL_BGRA;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_RGBA;
    if (texStorage) {
      internalFormat = GL_BGRA8_EXT;
    } else if (texImage && deviceFeatures.hasExtension(Extensions::TextureFormatBgra8888Ext)) {
      internalFormat = GL_BGRA;
    } else if (texImage &&
               deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageBgraRgba8)) {
      internalFormat = GL_RGBA8;
    }
    return true;

  case TextureFormat::BGRA_UNorm8_Rev:
    internalFormat = GL_RGBA;
    format = GL_BGRA;
    type = GL_UNSIGNED_INT_8_8_8_8_REV;
    return true;

  case TextureFormat::RGB10_A2_UNorm_Rev:
    format = GL_RGBA;
    type = GL_UNSIGNED_INT_2_10_10_10_REV;
    internalFormat = GL_RGB10_A2;
    if (texImage &&
        deviceFeatures.hasInternalRequirement(InternalRequirement::ColorTexImageRgb10A2Unsized)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::RGB10_A2_Uint_Rev:
    internalFormat = GL_RGB10_A2UI;
    format = GL_RGBA_INTEGER;
    type = GL_UNSIGNED_INT_2_10_10_10_REV;
    return true;

  case TextureFormat::BGR10_A2_Unorm:
    format = GL_BGRA;
    type = GL_UNSIGNED_INT_2_10_10_10_REV;
    internalFormat = GL_RGB10_A2;
    return true;

  case TextureFormat::ABGR_UNorm4: // TODO Test this
    format = GL_RGBA;
    type = GL_UNSIGNED_SHORT_4_4_4_4;
    internalFormat = GL_RGBA4;
    if (texImage &&
        deviceFeatures.hasInternalRequirement(InternalRequirement::ColorTexImageRgba4Unsized)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::B5G5R5A1_UNorm:
    format = GL_BGRA;
    type = GL_UNSIGNED_SHORT_5_5_5_1;
    internalFormat = GL_RGB5_A1;
    if (texImage &&
        deviceFeatures.hasInternalRequirement(InternalRequirement::ColorTexImageRgb5A1Unsized)) {
      internalFormat = GL_RGBA;
    }
    return true;

  case TextureFormat::B5G6R5_UNorm:
    return false;

  case TextureFormat::LA_UNorm8:
    format = GL_LUMINANCE_ALPHA;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_LUMINANCE_ALPHA;
    if ((texStorage && deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexStorageLa8)) ||
        (texImage && deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageLa8))) {
      internalFormat = GL_LUMINANCE8_ALPHA8;
    }
    return true;

  case TextureFormat::RG_UNorm8:
    format = GL_RG;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_RG8;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageRg8)) {
      internalFormat = GL_RG;
    }
    return true;

  case TextureFormat::RG_F16:
    format = GL_RG;
    if (deviceFeatures.hasInternalRequirement(InternalRequirement::TextureHalfFloatExtReq)) {
      type = GL_HALF_FLOAT_OES; // NOTE: NOT the same as GL_HALF_FLOAT
    } else {
      type = GL_HALF_FLOAT;
    }
    internalFormat = GL_RG16F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage16f)) {
      internalFormat = GL_RG;
    }
    return true;

  case TextureFormat::RG_F32:
    format = GL_RG;
    type = GL_FLOAT;
    internalFormat = GL_RG32F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage32f)) {
      internalFormat = GL_RG;
    }
    return true;

  case TextureFormat::RG_UInt16:
    internalFormat = GL_RG16UI;
    format = GL_RG_INTEGER;
    type = GL_UNSIGNED_SHORT;
    return true;

  case TextureFormat::RG_UNorm16:
    internalFormat = GL_RG16;
    format = GL_RG;
    type = GL_UNSIGNED_SHORT;
    return true;

  case TextureFormat::RGBA_UInt32:
    internalFormat = GL_RGBA32UI;
    format = GL_RGBA_INTEGER;
    type = GL_UNSIGNED_INT;
    return true;

  case TextureFormat::A_UNorm8:
    type = GL_UNSIGNED_BYTE;
    format = GL_ALPHA;
    internalFormat = GL_ALPHA;
    if ((texImage && deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageA8)) ||
        (texStorage && deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexStorageA8))) {
      if (deviceFeatures.hasInternalRequirement(InternalRequirement::SwizzleAlphaTexturesReq)) {
        // GL_ALPHA was deprecated in GL3 so use GL_RED and use GL_TEXTURE_SWIZZLE_A in
        // swapTextureChannelsForFormat before calling texImage2D or texStorage2D
        internalFormat = GL_R8;
        format = GL_RED;
      } else {
        internalFormat = GL_ALPHA8;
      }
    }
    return true;

  case TextureFormat::L_UNorm8:
    format = GL_LUMINANCE;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_LUMINANCE;
    if ((texStorage && deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexStorageLa8)) ||
        (texImage && deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageLa8))) {
      internalFormat = GL_LUMINANCE8;
    }
    return true;

  case TextureFormat::R_UNorm8:
    format = GL_RED;
    type = GL_UNSIGNED_BYTE;
    internalFormat = GL_R8;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImageRg8)) {
      internalFormat = GL_RED;
    }
    return true;

  case TextureFormat::R_F16:
    format = GL_RED;
    if (deviceFeatures.hasInternalRequirement(InternalRequirement::TextureHalfFloatExtReq)) {
      type = GL_HALF_FLOAT_OES; // NOTE: NOT the same as GL_HALF_FLOAT
    } else {
      type = GL_HALF_FLOAT;
    }
    internalFormat = GL_R16F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage16f)) {
      internalFormat = GL_RED;
    }
    return true;

  case TextureFormat::R_F32:
    format = GL_RED;
    type = GL_FLOAT;
    internalFormat = GL_R32F;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::ColorTexImage32f)) {
      internalFormat = GL_RED;
    }
    return true;

  case TextureFormat::R_UInt16:
    internalFormat = GL_R16UI;
    format = GL_RED_INTEGER;
    type = GL_UNSIGNED_SHORT;
    return true;

  case TextureFormat::R_UNorm16:
    internalFormat = GL_R16;
    format = GL_RED;
    type = GL_UNSIGNED_SHORT;
    return true;

  case TextureFormat::RGBA_ASTC_4x4:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_4x4:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_5x4:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_5x4_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_5x4:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_5x5:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_5x5:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_6x5:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_6x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_6x5:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_6x6:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_6x6:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_8x5:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_8x5:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_8x6:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_8x6_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_8x6:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_8x8:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_8x8:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_10x5:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_10x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_10x5:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_10x6:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_10x6_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_10x6:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_10x8:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_10x8_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_10x8:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_10x10:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_10x10_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_10x10:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_12x10:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_12x10_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_12x10:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_ASTC_12x12:
    internalFormat = GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_ASTC_12x12:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_BC7_UNORM_4x4:
    internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_BC7_SRGB_4x4:
    internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_PVRTC_2BPPV1:
    internalFormat = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGB_PVRTC_2BPPV1:
    internalFormat = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA_PVRTC_4BPPV1:
    internalFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGB_PVRTC_4BPPV1:
    internalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGB8_ETC1:
    internalFormat = GL_ETC1_RGB8_OES;
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGB8_ETC2:
    internalFormat = GL_COMPRESSED_RGB8_ETC2;
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
    internalFormat = GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RGBA8_EAC_ETC2:
    internalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_ETC2:
    internalFormat = GL_COMPRESSED_SRGB8_ETC2;
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
    internalFormat = GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::SRGB8_A8_EAC_ETC2:
    internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RG_EAC_UNorm:
    internalFormat = GL_COMPRESSED_RG11_EAC;
    format = GL_RG;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::RG_EAC_SNorm:
    internalFormat = GL_COMPRESSED_SIGNED_RG11_EAC;
    format = GL_RG;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::R_EAC_UNorm:
    internalFormat = GL_COMPRESSED_R11_EAC;
    format = GL_RED;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::R_EAC_SNorm:
    internalFormat = GL_COMPRESSED_SIGNED_R11_EAC;
    format = GL_RED;
    type = GL_UNSIGNED_BYTE;
    return compressedValid;

  case TextureFormat::S8_UInt_Z32_UNorm:
    // TODO: Fix this texture type. No backend has a 32-bit int depth + 8-bit int stencil.
    internalFormat = GL_DEPTH32F_STENCIL8;
    format = GL_DEPTH_STENCIL;
    type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    return true;

  case TextureFormat::S_UInt8:
    // TODO: test this
    internalFormat = GL_STENCIL_INDEX8;
    format = GL_STENCIL_INDEX;
    type = GL_UNSIGNED_BYTE;
    return true;

  case TextureFormat::Z_UNorm16:
    format = GL_DEPTH_COMPONENT;
    type = GL_UNSIGNED_SHORT;
    internalFormat = GL_DEPTH_COMPONENT16;
    if (texImage && !deviceFeatures.hasTextureFeature(TextureFeatures::DepthTexImage16)) {
      internalFormat = GL_DEPTH_COMPONENT;
    }
    return true;

  case TextureFormat::Z_UNorm32:
    format = GL_DEPTH_COMPONENT;
    type = GL_UNSIGNED_INT;
    internalFormat = GL_DEPTH_COMPONENT32;
    if (texImage && deviceFeatures.hasInternalRequirement(InternalRequirement::Depth32Unsized)) {
      internalFormat = GL_DEPTH_COMPONENT;
    }
    return true;

  case TextureFormat::Z_UNorm24:
    format = GL_DEPTH_COMPONENT;
    type = GL_UNSIGNED_INT;
    internalFormat = GL_DEPTH_COMPONENT24;
    return true;

  case TextureFormat::S8_UInt_Z24_UNorm:
    // Support for TextureBuffer and renderbuffer introduced with the same versions / extensions
    format = GL_DEPTH_STENCIL;
    type = GL_UNSIGNED_INT_24_8;
    internalFormat = GL_DEPTH24_STENCIL8;
    if (texImage &&
        deviceFeatures.hasInternalRequirement(InternalRequirement::Depth24Stencil8Unsized)) {
      internalFormat = GL_DEPTH_STENCIL;
    }
    return true;
  case TextureFormat::YUV_NV12:
    return false;
  }

  return false;
}

} // namespace opengl
} // namespace igl
