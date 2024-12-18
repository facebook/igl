/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <CoreVideo/CVMetalTextureCache.h>
#import <CoreVideo/CVPixelBuffer.h>
#import <Metal/Metal.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAMetalLayer.h>
#include <igl/Common.h>
#include <igl/PlatformDevice.h>
#include <igl/Texture.h>
#include <igl/metal/SamplerState.h>

namespace igl {

struct FramebufferDesc;

namespace metal {

class Device;
class Framebuffer;

class PlatformDevice final : public IPlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::Metal;

  PlatformDevice(Device& device);
  ~PlatformDevice() override;

  std::shared_ptr<SamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                   Result* outResult) const;
  std::shared_ptr<Framebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                 Result* outResult) const;

  /// Creates a texture from a native drawable
  /// @param nativeDrawable drawable. For Metal, drawable MUST be CAMetalDrawable
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::unique_ptr<ITexture> createTextureFromNativeDrawable(id<CAMetalDrawable> nativeDrawable,
                                                            Result* outResult);

  /// Creates a texture from a native drawable
  /// @param nativeDrawable drawable. For Metal, drawable MUST be MTLTexture
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::unique_ptr<ITexture> createTextureFromNativeDrawable(id<MTLTexture> nativeDrawable,
                                                            Result* outResult);

  /// Creates a texture from a native drawable surface
  /// @param nativeDrawable drawable surface. For Metal, drawable MUST be CAMetalLayer
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::unique_ptr<ITexture> createTextureFromNativeDrawable(CALayer* nativeDrawable,
                                                            Result* outResult);

  /// Creates a depth texture from a native depth stencil texture
  /// @param depthStencilTexture Native depth stencil texture.
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::unique_ptr<ITexture> createTextureFromNativeDepth(id<MTLTexture> depthStencilTexture,
                                                         Result* outResult);

  /// Creates a texture from a native PixelBuffer. Uses the backing PixelBuffer for width and
  /// height.
  /// @param sourceImage source image
  /// @param format the format of the source texture
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::unique_ptr<ITexture> createTextureFromNativePixelBuffer(CVImageBufferRef sourceImage,
                                                               TextureFormat format,
                                                               size_t planeIndex,
                                                               Result* outResult);

  /// Creates a texture from a native PixelBuffer
  /// @param sourceImage source image
  /// @param format the format of the source texture
  /// @param width the width of the texture
  /// @param height the height of the texture
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::unique_ptr<ITexture> createTextureFromNativePixelBufferWithSize(CVImageBufferRef sourceImage,
                                                                       TextureFormat format,
                                                                       size_t width,
                                                                       size_t height,
                                                                       size_t planeIndex,
                                                                       Result* outResult);

  /// Get a size of a given native drawable surface.
  /// @param nativeDrawable drawable surface. For Metal, drawable MUST be CAMetalLayer
  /// @param outResult Optional result.
  /// @return An accurate size that is suitable for rendering or zero if failed to get a size.
  Size getNativeDrawableSize(CALayer* nativeDrawable, Result* outResult);

  /// Get a texture format that is suitable to render a given drawable surface.
  /// @param nativeDrawable drawable surface. For Metal, drawable MUST be CAMetalLayer
  /// @param outResult Optional result.
  /// @return An accurate pixel format that is suitable for rendering or invalid if failed.
  TextureFormat getNativeDrawableTextureFormat(CALayer* nativeDrawable, Result* outResult);

  void flushNativeTextureCache() const;

  CVMetalTextureCacheRef getTextureCache();

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override {
    return t == Type;
  }

 private:
  Device& device_;
  CVMetalTextureCacheRef textureCache_ = nullptr;
};

} // namespace metal
} // namespace igl
