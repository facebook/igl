/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#ifndef __EMSCRIPTEN__
#error "Platform not supported"
#endif

#include <igl/Texture.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/PlatformDevice.h>

namespace igl::opengl {

class ViewTextureTarget;

namespace webgl {

class Device;
class Context;

class PlatformDevice : public opengl::PlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::OpenGLWebGL;

  PlatformDevice(Device& owner);
  ~PlatformDevice() override = default;

  /// Returns a texture representing the EGL Surface associated with this device's context.
  std::shared_ptr<ITexture> createTextureFromNativeDrawable(int width,
                                                            int height,
                                                            Result* outResult);

 protected:
  bool isType(PlatformDeviceType t) const noexcept override;

 private:
  std::shared_ptr<ViewTextureTarget> drawableTexture_;
};

} // namespace webgl
} // namespace igl::opengl
