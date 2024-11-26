/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <igl/Macros.h>
#include <igl/Texture.h>
#include <igl/metal/CommandQueue.h>

#if IGL_PLATFORM_APPLE
NS_ASSUME_NONNULL_BEGIN
#endif

namespace igl::metal {

class Texture final : public ITexture {
  friend class Device;
  friend class PlatformDevice;

 public:
  Texture(id<MTLTexture> texture, const ICapabilities& capabilities);
  Texture(id<CAMetalDrawable> drawable, const ICapabilities& capabilities);
  ~Texture() override;

  Result getBytes(const TextureRangeDesc& range, void* outData, size_t bytesPerRow = 0) const;

  // Accessors
  [[nodiscard]] Dimensions getDimensions() const override;
  [[nodiscard]] uint32_t getNumLayers() const override;
  [[nodiscard]] TextureType getType() const override;
  [[nodiscard]] TextureDesc::TextureUsage getUsage() const override;
  [[nodiscard]] uint32_t getSamples() const override;
  [[nodiscard]] uint32_t getNumMipLevels() const override;
  void generateMipmap(ICommandQueue& cmdQueue,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  [[nodiscard]] bool isRequiredGenerateMipmap() const override;
  [[nodiscard]] uint64_t getTextureId() const override;

  IGL_INLINE id<MTLTexture> _Nullable get() const {
    return (drawable_) ? drawable_.texture : value_;
  }
  IGL_INLINE id<CAMetalDrawable> _Nullable getDrawable() const {
    return drawable_;
  }

  static TextureDesc::TextureUsage toTextureUsage(MTLTextureUsage usage);
  static MTLTextureUsage toMTLTextureUsage(TextureDesc::TextureUsage usage);

  static MTLPixelFormat textureFormatToMTLPixelFormat(TextureFormat value);
  static TextureFormat mtlPixelFormatToTextureFormat(MTLPixelFormat value);
  static MTLTextureType convertType(TextureType value, size_t numSamples);
  static TextureType convertType(MTLTextureType value);
  static NSUInteger getMetalSlice(TextureType type, uint32_t face, uint32_t layer);
  static TextureRangeDesc atMetalSlice(TextureType type,
                                       const TextureRangeDesc& range,
                                       NSUInteger metalSlice);

 private:
  [[nodiscard]] bool needsRepacking(const TextureRangeDesc& range, size_t bytesPerRow) const final;
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow) const final;

  void generateMipmap(id<MTLCommandBuffer> cmdBuffer) const;

  // Given bytes per row of an input texture, return bytesPerRow value
  // accepted by Texture::upload and MTL replaceRegion.
  [[nodiscard]] size_t toMetalBytesPerRow(size_t bytesPerRow) const;

  id<MTLTexture> _Nullable value_;
  id<CAMetalDrawable> _Nullable drawable_;
  const ICapabilities& capabilities_;
};

} // namespace igl::metal

#if IGL_PLATFORM_APPLE
NS_ASSUME_NONNULL_END
#endif
