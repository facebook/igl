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
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::samples {

enum class BackendTypeID { GLES3, GLES2, Vulkan };

class TinyRenderer final {
 public:
  void init(AAssetManager* mgr, ANativeWindow* nativeWindow, BackendTypeID backendTypeID);
  void render(float displayScale);
  void onSurfacesChanged(ANativeWindow* nativeWindow, int width, int height);
  void onSurfaceDestroyed(ANativeWindow* surface);
  void touchEvent(bool isDown, float x, float y, float dx, float dy);

 private:
  BackendTypeID backendTypeID_;
  std::shared_ptr<igl::shell::PlatformAndroid> platform_;
  std::unique_ptr<igl::shell::RenderSession> session_;

  shell::ShellParams shellParams_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

} // namespace igl::samples
