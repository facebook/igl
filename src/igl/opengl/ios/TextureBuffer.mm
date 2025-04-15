/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <Foundation/Foundation.h>

#include <igl/opengl/ios/TextureBuffer.h>

namespace igl::opengl::ios {
namespace {
/// The conversion from CVPixelFormatType to igl::TextureFormat is inferred from CVPixelBuffer.h
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
TextureFormat convertToTextureFormat(const DeviceFeatureSet& deviceFeatures,
                                     OSType pixelFormat,
                                     size_t planeIndex) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  switch (pixelFormat) {
  case kCVPixelFormatType_32BGRA:
    return TextureFormat::BGRA_UNorm8;

  case kCVPixelFormatType_64RGBAHalf:
    return TextureFormat::RGBA_F16;

  case kCVPixelFormatType_OneComponent8:
    return TextureFormat::R_UNorm8;

  case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
  case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
    const bool supportsRG = deviceFeatures.hasFeature(DeviceFeatures::TextureFormatRG);

    if (planeIndex == 0) {
      return supportsRG ? TextureFormat::R_UNorm8 : TextureFormat::A_UNorm8;
    } else if (planeIndex == 1) {
      return supportsRG ? TextureFormat::RG_UNorm8 : TextureFormat::LA_UNorm8;
    } else {
      return TextureFormat::Invalid;
    }
  }
  case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange: {
    if (planeIndex == 0) {
      return TextureFormat::R_UInt16;
    } else if (planeIndex == 1) {
      return TextureFormat::RG_UInt16;
    } else {
      return TextureFormat::Invalid;
    }
  }
  default:
    return TextureFormat::Invalid;
  }
}
} // namespace

TextureBuffer::TextureBuffer(IContext& context,
                             CVPixelBufferRef pixelBuffer,
                             CVOpenGLESTextureCacheRef textureCache,
                             // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                             size_t planeIndex,
                             TextureDesc::TextureUsage usage) :
  Super(context,
        convertToTextureFormat(context.deviceFeatures(),
                               CVPixelBufferGetPixelFormatType(pixelBuffer),
                               planeIndex)),
  pixelBuffer_(CVPixelBufferRetain(pixelBuffer)),
  textureCache_((CVOpenGLESTextureCacheRef)CFRetain(textureCache)),
  planeIndex_(planeIndex) {
  setUsage(usage);
}

TextureBuffer::~TextureBuffer() {
  if (textureCache_) {
    CFRelease(textureCache_);
  }
  CVPixelBufferRelease(cvTexture_);
  CVPixelBufferRelease(pixelBuffer_);

#if TARGET_OS_SIMULATOR
  if (getFormat() == TextureFormat::BGRA_UNorm8) {
    // We only have a manually generated textureID when it's on simulator and BGRA
    getContext().deleteTextures({getId()});
  }
#endif
  setTextureBufferProperties(0, 0);
  setUsage(0);
}

Result TextureBuffer::create(const TextureDesc& /*desc*/, bool /*hasStorageAlready*/) {
  return Result(Result::Code::Unsupported,
                "igl::opengl::ios::TextureBuffer does not support this creation");
}

Result TextureBuffer::create() {
  const auto isPlanar = CVPixelBufferIsPlanar(pixelBuffer_);
  const size_t width = (isPlanar ? CVPixelBufferGetWidthOfPlane(pixelBuffer_, planeIndex_)
                                 : CVPixelBufferGetWidth(pixelBuffer_));
  const size_t height = (isPlanar ? CVPixelBufferGetHeightOfPlane(pixelBuffer_, planeIndex_)
                                  : CVPixelBufferGetHeight(pixelBuffer_));
  return TextureBuffer::createWithSize(width, height);
}

Result TextureBuffer::createWithSize(size_t width, size_t height) {
  if (pixelBuffer_ == nullptr || textureCache_ == nullptr) {
    return Result(Result::Code::ArgumentNull, "PixelBuffer or TextureCache is NULL");
  }

  if (isCreated_) {
    return Result(Result::Code::InvalidOperation,
                  "TextureBuffer has already been created with a pixelBuffer");
  }
  isCreated_ = true;

  if (uploaded_) {
    return Result();
  }
  uploaded_ = true;

  if (!toFormatDescGL(getFormat(), TextureDesc::TextureUsageBits::Sampled, formatDescGL_)) {
    return Result(Result::Code::ArgumentInvalid, "Invalid texture format");
  }
  if (getFormat() == TextureFormat::BGRA_UNorm8) {
    // TODO: Remove this once unit tests verify this is not needed.
    // Override BGRA internal format since below functions rely on TexImage2D and not TexStorage2D.
    formatDescGL_.internalFormat = GL_RGBA;
  }
  setTextureProperties(width, height);

#if TARGET_OS_SIMULATOR
  if (getFormat() == TextureFormat::BGRA_UNorm8) {
    // The behavior of CVOpenGLESTextureCacheCreateTextureFromImage() changed
    // in iOS 13 simulator, and it fails for unknown reasons when BGRA only.
    // So we have to create and upload the texture ourselves.
    GLuint textureID = 0;
    getContext().genTextures(1, &textureID);
    getContext().bindTexture(GL_TEXTURE_2D, textureID);

    GLint prevUnpackAlignment = -1;
    getContext().getIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpackAlignment);

    const size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer_);
    getContext().pixelStorei(GL_UNPACK_ALIGNMENT, this->getAlignment(bytesPerRow));

    BOOL requiresLineByLineUpload = NO;
    const auto bytesPerPixel = getProperties().bytesPerBlock;
    if (bytesPerRow / bytesPerPixel > width) {
      // Alignment alone can't make this work, but setting the row length is only available
      // in ES3. The fallback is to upload the texture line-by-line.
      if (getContext().deviceFeatures().getGLVersion() >= GLVersion::v3_0_ES) {
        getContext().pixelStorei(GL_UNPACK_ROW_LENGTH, bytesPerRow / bytesPerPixel);
      } else {
        requiresLineByLineUpload = YES;
      }
    }

    CVPixelBufferLockBaseAddress(pixelBuffer_, 0);
    void* imageData = CVPixelBufferGetBaseAddress(pixelBuffer_);
    if (requiresLineByLineUpload) {
      // Before we can update individual lines, the texture must be fully allocated.
      getContext().texImage2D(
          GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

      for (size_t currentLine = 0; currentLine < height; ++currentLine) {
        void* offsetData =
            static_cast<void*>(static_cast<unsigned char*>(imageData) + bytesPerRow * currentLine);
        getContext().texSubImage2D(
            GL_TEXTURE_2D, 0, 0, currentLine, width, 1, GL_BGRA, GL_UNSIGNED_BYTE, offsetData);
      }
    } else {
      getContext().texImage2D(
          GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, imageData);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer_, 0);

    if (getContext().deviceFeatures().getGLVersion() >= GLVersion::v3_0_ES) {
      getContext().pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    getContext().pixelStorei(GL_UNPACK_ALIGNMENT, prevUnpackAlignment);

    setTextureBufferProperties(textureID, GL_TEXTURE_2D);

    return Result();
  }
#endif
  const auto error = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                  textureCache_,
                                                                  pixelBuffer_,
                                                                  nullptr,
                                                                  GL_TEXTURE_2D,
                                                                  formatDescGL_.internalFormat,
                                                                  width,
                                                                  height,
                                                                  formatDescGL_.format,
                                                                  formatDescGL_.type,
                                                                  planeIndex_,
                                                                  &cvTexture_);
  if (error != noErr) {
    return Result(Result::Code::Unsupported,
                  "Failed to create CVOpenGLESTexture: " + std::to_string(error) +
                      " internalFormat: " + std::to_string(formatDescGL_.internalFormat) +
                      " format: " + std::to_string(formatDescGL_.format) +
                      " type: " + std::to_string(formatDescGL_.type));
  }

  setTextureBufferProperties(CVOpenGLESTextureGetName(cvTexture_),
                             CVOpenGLESTextureGetTarget(cvTexture_));

  return Result();
}

bool TextureBuffer::supportsUpload() const {
  return false;
}

Result TextureBuffer::uploadInternal(TextureType /*type*/,
                                     const TextureRangeDesc& /*range*/,
                                     const void* /*data*/,
                                     size_t /*bytesPerRow*/) const {
  return Result();
}

} // namespace igl::opengl::ios
