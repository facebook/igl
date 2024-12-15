/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Config.h>

// clang-format off
#if IGL_PLATFORM_WINDOWS
  #include <igl/opengl/wgl/Context.h>
  #include <igl/opengl/wgl/Device.h>
#elif IGL_PLATFORM_LINUX
  #include <igl/opengl/glx/Context.h>
  #include <igl/opengl/glx/Device.h>
#endif
// clang-format on

namespace igl::shell::util {

#if IGL_PLATFORM_WINDOWS
struct WGLDevice final : public igl::opengl::Device {
  explicit WGLDevice(std::unique_ptr<igl::opengl::IContext> context) :
    Device(std::move(context)), platformDevice_(*this) {}

  const igl::opengl::PlatformDevice& getPlatformDevice() const noexcept override {
    return platformDevice_;
  }

  void endScope() override {
    Device::endScope();
    if (!verifyScope()) {
      wglMakeCurrent(NULL, NULL);
    }
  }

  igl::opengl::PlatformDevice platformDevice_;
};
#elif IGL_PLATFORM_LINUX
struct GLXDevice final : public igl::opengl::Device {
  explicit GLXDevice(std::unique_ptr<igl::opengl::glx::Context> context) :
    Device(std::move(context)), platformDevice_(*this) {}

  const igl::opengl::PlatformDevice& getPlatformDevice() const noexcept override {
    return platformDevice_;
  }

  void endScope() override {
    Device::endScope();
    if (!verifyScope()) {
      getContext().clearCurrentContext();
    }
  }

  igl::opengl::PlatformDevice platformDevice_;
};
#endif
} // namespace igl::shell::util
