/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/mobile/opengl/XrAppImplGLES.h>

#include <igl/HWDevice.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/egl/Context.h>
#include <igl/opengl/egl/HWDevice.h>

#include <shell/openxr/XrLog.h>
#include <shell/openxr/mobile/opengl/XrSwapchainProviderImplGLES.h>

namespace igl::shell::openxr::mobile {
std::vector<const char*> XrAppImplGLES::getXrRequiredExtensions() const {
  return {
      XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
      XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
#if defined(IGL_CMAKE_BUILD)
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
#endif
  };
}

void* XrAppImplGLES::getInstanceCreateExtension() {
#if defined(IGL_CMAKE_BUILD)
  return &instanceCreateInfoAndroid_;
#else
  return nullptr;
#endif
}

std::unique_ptr<igl::IDevice> XrAppImplGLES::initIGL(XrInstance instance, XrSystemId systemId) {
  // Get the graphics requirements.
  PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrGetOpenGLESGraphicsRequirementsKHR",
                                 (PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

  XR_CHECK(pfnGetOpenGLESGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements_));

  Result result;
  igl::HWDeviceQueryDesc queryDesc(HWDeviceType::IntegratedGpu);
  auto hwDevice = opengl::egl::HWDevice();
  auto hwDevices = hwDevice.queryDevices(queryDesc, &result);
  IGL_ASSERT(result.isOk());
  return hwDevice.create(hwDevices[0], igl::opengl::RenderingAPI::GLES3, nullptr, &result);
}

XrSession XrAppImplGLES::initXrSession(XrInstance instance,
                                       XrSystemId systemId,
                                       igl::IDevice& device) {
  const auto& glDevice = static_cast<igl::opengl::Device&>(device); // Downcast is safe here
  const auto& context = static_cast<igl::opengl::egl::Context&>(glDevice.getContext());

  XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingAndroidGLES = {
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
      .next = nullptr,
      .display = context.getDisplay(),
      .config = context.getConfig(),
      .context = context.get(),
  };

  XrSessionCreateInfo sessionCreateInfo = {
      .type = XR_TYPE_SESSION_CREATE_INFO,
      .next = &graphicsBindingAndroidGLES,
      .createFlags = 0,
      .systemId = systemId,
  };

  XrResult xrResult;
  XrSession session;
  XR_CHECK(xrResult = xrCreateSession(instance, &sessionCreateInfo, &session));
  if (xrResult != XR_SUCCESS) {
    IGL_LOG_ERROR("Failed to create XR session: %d.", xrResult);
    return XR_NULL_HANDLE;
  }
  IGL_LOG_INFO("XR session created");

  return session;
}

std::unique_ptr<impl::XrSwapchainProviderImpl> XrAppImplGLES::createSwapchainProviderImpl() const {
  return std::make_unique<XrSwapchainProviderImplGLES>();
}
} // namespace igl::shell::openxr::mobile
