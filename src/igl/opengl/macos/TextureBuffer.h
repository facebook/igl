/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <CoreVideo/CVOpenGLTextureCache.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <igl/opengl/TextureBuffer.h>

namespace igl::opengl::macos {

class TextureBuffer final : public opengl::TextureBuffer {
  using Super = opengl::TextureBuffer;

 public:
  TextureBuffer(IContext& context,
                CVPixelBufferRef pixelBuffer,
                CVOpenGLTextureCacheRef textureCache,
                TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled);
  ~TextureBuffer() override;

  // Disable those creation methods
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;

  // This function is created to support wrapping an igl::ITexture container
  // around a GL texture created from CVOpenGLTextureCacheCreateTextureFromImage()
  Result create();

  bool supportsUpload() const final;

 private:
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* data,
                        size_t bytesPerRow,
                        const uint32_t* IGL_NULLABLE mipLevelBytes) const final;

  CVOpenGLTextureRef cvTexture_ = nullptr;
  CVPixelBufferRef pixelBuffer_ = nullptr;
  CVOpenGLTextureCacheRef textureCache_ = nullptr;
  bool uploaded_ = false;
  bool created_ = false;
};

} // namespace igl::opengl::macos
