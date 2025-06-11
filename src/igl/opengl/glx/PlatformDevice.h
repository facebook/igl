/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Texture.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/PlatformDevice.h>

#include <cstdint>

namespace igl::opengl {
class ViewTextureTarget;
}

namespace igl::opengl::glx {

class Device;
class Context;

class PlatformDevice : public opengl::PlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::OpenGLx;

  explicit PlatformDevice(Device& owner);
  ~PlatformDevice() override = default;

  /// Returns a texture representing the GLX Surface associated with this device's context.
  [[nodiscard]] std::shared_ptr<ITexture> createTextureFromNativeDrawable(uint32_t width,
                                                                          uint32_t height,
                                                                          Result* outResult);
  [[nodiscard]] std::shared_ptr<ITexture> createTextureFromNativeDepth(uint32_t width,
                                                                       uint32_t height,
                                                                       Result* outResult);

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override;

 private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  std::shared_ptr<ViewTextureTarget> drawableTexture_;
  std::shared_ptr<ViewTextureTarget> depthTexture_;
};

} // namespace igl::opengl::glx
