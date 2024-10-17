/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <igl/IGL.h>
#include <memory>
#include <shell/shared/platform/android/PlatformAndroid.h>
#include <shell/shared/renderSession/IRenderSessionFactory.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::samples {

class TinyRenderer final {
 public:
  void init(AAssetManager* mgr,
            ANativeWindow* nativeWindow,
            shell::IRenderSessionFactory& factor,
            BackendVersion backendVersion,
            TextureFormat swapchainColorTextureFormat);
  void recreateSwapchain(ANativeWindow* nativeWindow, bool createSurface); // only for Vulkan
  void render(float displayScale);
  void onSurfacesChanged(ANativeWindow* nativeWindow, int width, int height);
  void touchEvent(bool isDown, float x, float y, float dx, float dy);
  void setClearColorValue(float r, float g, float b, float a);

  [[nodiscard]] const BackendVersion& backendVersion() const noexcept {
    return backendVersion_;
  }

 private:
  BackendVersion backendVersion_;
  std::shared_ptr<igl::shell::PlatformAndroid> platform_;
  std::unique_ptr<igl::shell::RenderSession> session_;

  shell::ShellParams shellParams_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  ANativeWindow* nativeWindow_ = nullptr;
};

} // namespace igl::samples
