/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/macos/TextureBuffer.h>

namespace igl::opengl::macos {
namespace {
TextureFormat convertToTextureFormat(OSType pixelFormat) {
  switch (pixelFormat) {
  case kCVPixelFormatType_32BGRA:
    return TextureFormat::RGBA_UNorm8;

  case kCVPixelFormatType_OneComponent8:
    return TextureFormat::R_UNorm8;

  case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
  case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
    // On the iOS side, there's a plane index which will cause this case to
    // return different values based the index. On the MacOS side there does
    // not appear to be an equivalent index, so we are really just assuming
    // planeIndex == 0 here.
    return TextureFormat::R_UNorm8;
  }
  default:
    return TextureFormat::Invalid;
  }
}
}
TextureBuffer::TextureBuffer(IContext& context,
                             CVPixelBufferRef pixelBuffer,
                             CVOpenGLTextureCacheRef textureCache,
                             TextureDesc::TextureUsage usage) :
  Super(context, convertToTextureFormat(CVPixelBufferGetPixelFormatType(pixelBuffer))),
  pixelBuffer_(CVPixelBufferRetain(pixelBuffer)),
  textureCache_((CVOpenGLTextureCacheRef)CFRetain(textureCache)) {
  setUsage(usage);
}

TextureBuffer::~TextureBuffer() {
  if (textureCache_) {
    CFRelease(textureCache_);
  }
  CVPixelBufferRelease(cvTexture_);
  CVPixelBufferRelease(pixelBuffer_);

  setTextureBufferProperties(0, 0);
  setUsage(0);
}

Result TextureBuffer::create(const TextureDesc& /*desc*/, bool /*hasStorageAlready*/) {
  return Result(Result::Code::Unsupported,
                "igl::opengl::macos::TextureBuffer does not support this creation");
}

Result TextureBuffer::create() {
  if (pixelBuffer_ == nullptr || textureCache_ == nullptr) {
    return Result(Result::Code::ArgumentNull, "PixelBuffer or TextureCache is NULL");
  }

  if (created_) {
    return Result(Result::Code::InvalidOperation,
                  "TextureBuffer has already been created with a pixelBuffer");
  }
  created_ = true;

  if (uploaded_) {
    return Result();
  }
  uploaded_ = true;

  if (!toFormatDescGL(getFormat(), TextureDesc::TextureUsageBits::Sampled, formatDescGL_)) {
    return Result(Result::Code::ArgumentInvalid, "Invalid texture format");
  }

  const auto error = CVOpenGLTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, textureCache_, pixelBuffer_, nullptr, &cvTexture_);
  if (error != noErr) {
    return Result(Result::Code::Unsupported, "Failed to create CVOpenGLESTexture");
  }

  setTextureProperties(static_cast<GLsizei>(CVPixelBufferGetWidth(pixelBuffer_)),
                       static_cast<GLsizei>(CVPixelBufferGetHeight(pixelBuffer_)));

  // Note that CVOpenGLTextureGetTarget(cvTexture_) returns GL_TEXTURE_RECTANGLE
  // which is not something IGL explicitly supports.
  setTextureBufferProperties(CVOpenGLTextureGetName(cvTexture_),
                             CVOpenGLTextureGetTarget(cvTexture_));

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

} // igl::opengl::macos
