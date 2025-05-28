/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/opengl/TestDevice.h>

#include <igl/opengl/IContext.h>

#if IGL_PLATFORM_IOS
#include <igl/opengl/ios/HWDevice.h>
#elif IGL_PLATFORM_MACOSX
#include <igl/opengl/macos/HWDevice.h>
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX_USE_EGL
#include <igl/opengl/egl/HWDevice.h>
#elif IGL_PLATFORM_LINUX
#include <igl/opengl/glx/HWDevice.h>
#elif IGL_PLATFORM_WINDOWS
#if defined(FORCE_USE_ANGLE)
#include <igl/opengl/egl/HWDevice.h>
#else
#include <igl/opengl/wgl/HWDevice.h>
#endif // FORCE_USE_ANGLE
#else
#error "Unsupported testing platform"
#endif

namespace igl::tests::util::device::opengl {

namespace {
template<typename THWDevice>
std::shared_ptr<::igl::opengl::Device> createOffscreenDevice() {
  THWDevice hwDevice;
  auto context = hwDevice.createOffscreenContext(640, 380, nullptr);
  return hwDevice.createWithContext(std::move(context), nullptr);
}
} // namespace

//
// createTestDevice
//
// Used by clients to get an IGL device. The backend is determined by
// the IGL_BACKEND_TYPE compiler flag in the BUCK file
//
std::shared_ptr<IDevice> createTestDevice(std::optional<BackendVersion> requestedVersion) {
  std::shared_ptr<IDevice> iglDev = nullptr;

#if IGL_PLATFORM_IOS
  iglDev = requestedVersion ? ::igl::opengl::ios::HWDevice().create(*requestedVersion)
                            : ::igl::opengl::ios::HWDevice().create();
#elif IGL_PLATFORM_MACOSX
  iglDev = requestedVersion ? ::igl::opengl::macos::HWDevice().create(*requestedVersion)
                            : ::igl::opengl::macos::HWDevice().create();
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX_USE_EGL
  iglDev = createOffscreenDevice<::igl::opengl::egl::HWDevice>();
#elif IGL_PLATFORM_LINUX
  iglDev = createOffscreenDevice<::igl::opengl::glx::HWDevice>();
#elif IGL_PLATFORM_WINDOWS
#if defined(FORCE_USE_ANGLE)
  iglDev = createOffscreenDevice<::igl::opengl::egl::HWDevice>();
#else
  iglDev = createOffscreenDevice<::igl::opengl::wgl::HWDevice>();
#endif // FORCE_USE_ANGLE
#else

#endif

  return iglDev;
}

} // namespace igl::tests::util::device::opengl
