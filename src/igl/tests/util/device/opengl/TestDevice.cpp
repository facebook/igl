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
std::shared_ptr<::igl::opengl::Device> createDevice(igl::opengl::RenderingAPI renderingAPI) {
  THWDevice hwDevice;
  auto context = hwDevice.createContext(renderingAPI, IGL_EGL_NULL_WINDOW, nullptr);
  return hwDevice.createWithContext(std::move(context), nullptr);
}
template<typename THWDevice>
std::shared_ptr<::igl::opengl::Device> createOffscreenDevice(
    igl::opengl::RenderingAPI renderingAPI) {
  THWDevice hwDevice;
  auto context = hwDevice.createOffscreenContext(renderingAPI, 640, 380, nullptr);
  return hwDevice.createWithContext(std::move(context), nullptr);
}
} // namespace

igl::opengl::RenderingAPI getOpenGLRenderingAPI(const std::string& backendApi) {
  auto renderingAPI = backendApi == "2.0" ? ::igl::opengl::RenderingAPI::GLES2
                                          : ::igl::opengl::RenderingAPI::GLES3;

#if IGL_PLATFORM_ANDROID && !defined(GL_ES_VERSION_3_0)
  renderingAPI = ::igl::opengl::RenderingAPI::GLES2;
#endif

  return renderingAPI;
}

//
// createTestDevice
//
// Used by clients to get an IGL device. The backend is determined by
// the IGL_BACKEND_TYPE compiler flag in the BUCK file
//
std::shared_ptr<IDevice> createTestDevice(const std::string& backendApi) {
  std::shared_ptr<IDevice> iglDev = nullptr;
  auto renderingAPI = getOpenGLRenderingAPI(backendApi);

#if IGL_PLATFORM_IOS
  iglDev = createDevice<::igl::opengl::ios::HWDevice>(renderingAPI);
#elif IGL_PLATFORM_MACOSX
  iglDev = createDevice<::igl::opengl::macos::HWDevice>(renderingAPI);
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX_USE_EGL
  iglDev = createOffscreenDevice<::igl::opengl::egl::HWDevice>(renderingAPI);
#elif IGL_PLATFORM_LINUX
  iglDev = createOffscreenDevice<::igl::opengl::glx::HWDevice>(renderingAPI);
#elif IGL_PLATFORM_WINDOWS
#if defined(FORCE_USE_ANGLE)
  iglDev = createOffscreenDevice<::igl::opengl::egl::HWDevice>(renderingAPI);
#else
  iglDev = createOffscreenDevice<::igl::opengl::wgl::HWDevice>(renderingAPI);
#endif // FORCE_USE_ANGLE
#else

#endif

  return std::static_pointer_cast<IDevice>(iglDev);
}

} // namespace igl::tests::util::device::opengl
