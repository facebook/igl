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
#if IGL_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace igl {
namespace opengl {

class ViewTextureTarget;

namespace wgl {

class Device;
class Context;

class PlatformDevice : public opengl::PlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::OpenGLWgl;

  PlatformDevice(Device& owner);
  ~PlatformDevice() override = default;

  /// Returns a texture representing the WGL Surface associated with this device's context.
  std::shared_ptr<ITexture> createTextureFromNativeDrawable(Result* outResult);
  std::shared_ptr<ITexture> createTextureFromNativeDepth(int width, int height, Result* outResult);

 protected:
  bool isType(PlatformDeviceType t) const noexcept override;

 private:
  std::shared_ptr<ViewTextureTarget> drawableTexture_;
  RECT dimension_;
};

} // namespace wgl
} // namespace opengl
} // namespace igl
