/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#if defined __OBJC__
#import <CoreVideo/CVOpenGLESTextureCache.h>
#import <CoreVideo/CVPixelBuffer.h>
#include <QuartzCore/CAEAGLLayer.h>
#else
typedef void CAEAGLLayer;
#endif
#include <CoreVideo/CVImageBuffer.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
#include <igl/Texture.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/PlatformDevice.h>

namespace igl::opengl::ios {

class Device;

class PlatformDevice final : public opengl::PlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType kType = igl::PlatformDeviceType::OpenGLIOS;

  PlatformDevice(Device& owner);
  ~PlatformDevice() override;

  std::shared_ptr<ITexture> createTextureFromNativeDrawable(CAEAGLLayer* nativeDrawable,
                                                            Result* outResult);
  std::shared_ptr<ITexture> createTextureFromNativeDepth(CAEAGLLayer* nativeDrawable,
                                                         TextureFormat depthTextureFormat,
                                                         Result* outResult);

  Size getNativeDrawableSize(CAEAGLLayer* nativeDrawable, Result* outResult);
  TextureFormat getNativeDrawableTextureFormat(CAEAGLLayer* nativeDrawable, Result* outResult);

  /// If the EAGLContext is created outside of IGL, then you would have to create the texture cache
  /// outside of IGL and pass it into this function. If the EAGLContext is created by IGL, then use
  /// the function below that would use IGL's internally created texture cache. It's important that
  /// the texture cache passed in, is generated by the current EAGLContext. Creates a texture from a
  /// native PixelBuffer.
  /// @param sourceImage source image
  /// @param textureCache an OpenGLES texture cache
  /// @param width width of generated texture
  /// @param height height of generated texture
  /// @param planeIndex the plane index to generate the texture
  /// @param outResult optional result
  /// @return pointer to generated TextureBuffer or nullptr
  std::unique_ptr<ITexture> createTextureFromNativePixelBufferWithSize(
      const CVImageBufferRef& sourceImage,
      const CVOpenGLESTextureCacheRef& textureCache,
      size_t width,
      size_t height,
      size_t planeIndex = 0,
      TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled,
      Result* outResult = nullptr);
  /// Uses the backing CVPixelBufferRef width and height.
  std::unique_ptr<ITexture> createTextureFromNativePixelBuffer(
      const CVImageBufferRef& sourceImage,
      const CVOpenGLESTextureCacheRef& textureCache,
      size_t planeIndex = 0,
      TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled,
      Result* outResult = nullptr);

  /// Creates a texture from a native PixelBuffer.
  /// @param sourceImage source image
  /// @param planeIndex the plane index to generate the texture
  /// @param outResult optional result
  /// @return pointer to generated TextureBuffer or nullptr
  std::unique_ptr<ITexture> createTextureFromNativePixelBufferWithSize(
      const CVImageBufferRef& sourceImage,
      size_t width,
      size_t height,
      size_t planeIndex = 0,
      TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled,
      Result* outResult = nullptr);
  /// Uses the backing CVPixelBufferRef width and height.
  std::unique_ptr<ITexture> createTextureFromNativePixelBuffer(
      const CVImageBufferRef& sourceImage,
      size_t planeIndex = 0,
      TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled,
      Result* outResult = nullptr);

  CVOpenGLESTextureCacheRef getTextureCache();

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override;
};

} // namespace igl::opengl::ios
