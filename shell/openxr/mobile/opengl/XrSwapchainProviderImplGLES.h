/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <android/native_window_jni.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_PLATFORM_ANDROID
#include <openxr/openxr_platform.h>

#include <shell/openxr/mobile/impl/XrSwapchainProviderImpl.h>

namespace igl::shell::openxr::mobile {
class XrSwapchainProviderImplGLES final : public impl::XrSwapchainProviderImpl {
 public:
  int64_t preferredColorFormat() const final {
    return GL_RGBA8;
  }
  int64_t preferredDepthFormat() const final {
    return GL_DEPTH_COMPONENT16;
  }
  void enumerateImages(igl::IDevice& device,
                       XrSwapchain colorSwapchain,
                       XrSwapchain depthSwapchain,
                       int64_t selectedColorFormat,
                       int64_t selectedDepthFormat,
                       const XrViewConfigurationView& viewport,
                       uint32_t numViews) final;
  igl::SurfaceTextures getSurfaceTextures(igl::IDevice& device,
                                          const XrSwapchain& colorSwapchain,
                                          const XrSwapchain& depthSwapchain,
                                          int64_t selectedColorFormat,
                                          int64_t selectedDepthFormat,
                                          const XrViewConfigurationView& viewport,
                                          uint32_t numViews) final;

 private:
  std::vector<uint32_t> colorImages_;
  std::vector<uint32_t> depthImages_;
};
} // namespace igl::shell::openxr::mobile
