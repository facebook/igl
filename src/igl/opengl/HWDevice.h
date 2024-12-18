/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/HWDevice.h>
#include <igl/Macros.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/IContext.h>

#if IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX || IGL_PLATFORM_EMSCRIPTEN
#include <EGL/eglplatform.h>
#elif IGL_PLATFORM_IOS || IGL_PLATFORM_MACOSX
#define EGLNativeWindowType void*
#elif IGL_ANGLE || defined(IGL_CMAKE_BUILD)
#include <EGL/eglplatform.h>
#else
#define EGLNativeWindowType void*
#endif

#if IGL_PLATFORM_LINUX
#define IGL_EGL_NULL_WINDOW 0L
#else
#define IGL_EGL_NULL_WINDOW nullptr
#endif

namespace igl::opengl {

class HWDevice {
 public:
  HWDevice() = default;

  virtual ~HWDevice() = default;

  virtual std::unique_ptr<IContext> createContext(opengl::RenderingAPI api,
                                                  EGLNativeWindowType nativeWindow,
                                                  Result* outResult) const = 0;

  virtual std::unique_ptr<Device> createWithContext(std::unique_ptr<IContext> context,
                                                    Result* outResult) const = 0;

  std::vector<HWDeviceDesc> queryDevices(const HWDeviceQueryDesc& desc, Result* outResult);

  std::unique_ptr<IDevice> create(const HWDeviceDesc& desc,
                                  igl::opengl::RenderingAPI api,
                                  EGLNativeWindowType nativeWindowType,
                                  Result* outResult = nullptr);
};

} // namespace igl::opengl
