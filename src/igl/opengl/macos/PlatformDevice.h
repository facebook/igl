/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <CoreVideo/CVOpenGLTextureCache.h>
#include <igl/Texture.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/PlatformDevice.h>

namespace igl::opengl {

class ViewTextureTarget;

namespace macos {

class Device;

class PlatformDevice : public opengl::PlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::OpenGLMacOS;

  PlatformDevice(Device& owner);
  ~PlatformDevice() override = default;

  /// Creates a texture that represents the default backbuffer for the view associated with the
  /// currently active OpenGL context. This is useful when using views like NSOpenGLView.
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::shared_ptr<ITexture> createTextureFromNativeDrawable(Result* outResult);

  /// Creates a texture that represents the default depth buffer for the view associated with the
  /// currently active OpenGL context. This is useful when using views like NSOpenGLView.
  /// @param outResult optional result
  /// @return pointer to generated Texture or nullptr
  std::shared_ptr<ITexture> createTextureFromNativeDepth(Result* outResult);

  /// Get a size of a given native drawable surface.
  /// @param outResult Optional result.
  /// @return An accurate size that is suitable for rendering or zero if failed to get a size.
  Size getNativeDrawableSize(Result* outResult);

  /// Get a texture format that is suitable to render a given drawable surface.
  /// @param outResult Optional result.
  /// @return An accurate pixel format that is suitable for rendering or invalid if failed.
  TextureFormat getNativeDrawableTextureFormat(Result* outResult);

  /// Set a texture format that is suitable to render a given drawable surface.
  /// @param format The pixel format that is suitable for rendering
  /// @param outResult Optional result.
  void setNativeDrawableTextureFormat(TextureFormat format, Result* outResult);

  /// Creates a texture from a native PixelBuffer.
  /// @param sourceImage source image
  /// @param an OpenGL texture cache
  /// @param outResult optional result
  /// @return pointer to generated TextureBuffer or nullptr
  std::unique_ptr<ITexture> createTextureFromNativePixelBuffer(
      const CVImageBufferRef& sourceImage,
      const CVOpenGLTextureCacheRef& textureCache,
      TextureDesc::TextureUsage usage = TextureDesc::TextureUsageBits::Sampled,
      Result* outResult = nullptr);

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override;

 private:
  std::shared_ptr<ViewTextureTarget> drawableTexture_;
  igl::TextureFormat drawableTextureFormat_ = igl::TextureFormat::RGBA_SRGB;
};
} // namespace macos
} // namespace igl::opengl
