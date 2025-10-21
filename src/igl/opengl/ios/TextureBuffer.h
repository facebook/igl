/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <CoreVideo/CVOpenGLESTextureCache.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <igl/opengl/TextureBuffer.h>

namespace igl::opengl::ios {

class TextureBuffer final : public opengl::TextureBuffer {
  using Super = opengl::TextureBuffer;

 public:
  /// @param pixelBuffer The backing CVPixelBufferRef source
  /// @param textureCache Texture cache
  /// @param planeIndex Plane index to generate texture
  /// @param usage Usage of the CVOpenGLESTextureRef
  TextureBuffer(IContext& context,
                CVPixelBufferRef IGL_NONNULL pixelBuffer,
                CVOpenGLESTextureCacheRef IGL_NONNULL textureCache,
                size_t planeIndex = 0,
                TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled);
  ~TextureBuffer() override;
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;
  TextureBuffer(TextureBuffer&&) = delete;
  TextureBuffer& operator=(TextureBuffer&&) = delete;

  // Disable those creation methods
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;

  /// Create a CVOpenGLESTextureRef with the given CVPixelBufferRef. Uses the backing
  /// CVPixelBufferRef width and height.
  Result create();

  /// Create a CVOpenGLESTextureRef with the given CVPixelBufferRef.
  /// @param width Width of generated texture
  /// @param height Height of generated texture
  Result createWithSize(size_t width, size_t height);

  bool supportsUpload() const final;

 private:
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow,
                        const uint32_t* IGL_NULLABLE mipLevelBytes) const final;

  CVOpenGLESTextureRef IGL_NULLABLE cvTexture_ = nullptr;
  CVPixelBufferRef IGL_NULLABLE pixelBuffer_ = nullptr;
  CVOpenGLESTextureCacheRef IGL_NULLABLE textureCache_ = nullptr;
  size_t planeIndex_ = 0;
  bool uploaded_ = false;
  bool isCreated_ = false;
};

} // namespace igl::opengl::ios
