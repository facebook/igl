/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <igl/Texture.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class Device;

/// @brief A WGPUSurface and the per-frame textures rendered to it.
///
/// getCurrentTexture() returns a texture whose WGPUTexture is acquired from the surface on its
/// first use as an attachment or copy source, i.e. after the last point where the frame could
/// suspend (a canvas texture expires when the browser regains control). Present it with
/// ICommandBuffer::present() before submitting: natively that calls wgpuSurfacePresent() after the
/// submit; in the browser the canvas presents on its own when the JS task ends (emdawnwebgpu
/// aborts in wgpuSurfacePresent()).
class Surface final {
 public:
  /// Adopts `surface`; `device` must outlive the surface.
  [[nodiscard]] static std::unique_ptr<Surface> create(Device& device,
                                                       Handle<WGPUSurface> surface,
                                                       Result* IGL_NULLABLE outResult = nullptr);
#if IGL_PLATFORM_APPLE && !IGL_PLATFORM_EMSCRIPTEN
  /// `layer` is a CAMetalLayer, which must outlive the surface.
  [[nodiscard]] static std::unique_ptr<Surface> createFromMetalLayer(
      Device& device,
      void* IGL_NONNULL layer,
      Result* IGL_NULLABLE outResult = nullptr);
#endif
#if IGL_PLATFORM_EMSCRIPTEN
  /// `selector` is the CSS selector of an HTML canvas, e.g. "#canvas".
  [[nodiscard]] static std::unique_ptr<Surface> createFromCanvas(
      Device& device,
      const char* IGL_NONNULL selector,
      Result* IGL_NULLABLE outResult = nullptr);
#endif
  ~Surface();

  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;
  Surface(Surface&&) = delete;
  Surface& operator=(Surface&&) = delete;

  /// (Re)configures the surface. With TextureFormat::Invalid, uses the surface's preferred format,
  /// which needs the device's adapter natively.
  [[nodiscard]] Result configure(uint32_t width,
                                 uint32_t height,
                                 TextureFormat format = TextureFormat::Invalid);
  /// The texture for the next frame; null before configure().
  [[nodiscard]] std::shared_ptr<ITexture> getCurrentTexture(
      Result* IGL_NULLABLE outResult = nullptr);

  [[nodiscard]] TextureFormat getFormat() const noexcept {
    return format_;
  }
  [[nodiscard]] uint32_t getWidth() const noexcept {
    return width_;
  }
  [[nodiscard]] uint32_t getHeight() const noexcept {
    return height_;
  }
  [[nodiscard]] WGPUSurface IGL_NULLABLE getWGPUSurface() const noexcept {
    return surface_.get();
  }

 private:
  Surface(Device& device, Handle<WGPUSurface> surface);

  Device& device_;
  Handle<WGPUSurface> surface_;
  TextureFormat format_ = TextureFormat::Invalid;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

} // namespace igl::webgpu
