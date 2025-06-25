/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

// clang-format off

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif // _WIN32

#if IGL_ANGLE
  #define GLFW_EXPOSE_NATIVE_EGL
#else
  #if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WGL
  #else
    #define GLFW_EXPOSE_NATIVE_GLX
  #endif
#endif // IGL_ANGLE
#define GLFW_INCLUDE_NONE

// clang-format on

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/RenderSessionConfig.h>
#include <shell/shared/renderSession/RenderSessionWindowConfig.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

class GlfwShell {
 public:
  GlfwShell();

  virtual ~GlfwShell() noexcept = default;

  bool initialize(int argc,
                  char* argv[],
                  RenderSessionWindowConfig suggestedWindowConfig,
                  const RenderSessionConfig& suggestedSessionConfig) noexcept;
  void run() noexcept;
  void teardown() noexcept;

 protected:
  ShellParams& shellParams() noexcept;
  [[nodiscard]] const ShellParams& shellParams() const noexcept;
  GLFWwindow* window() noexcept;
  [[nodiscard]] const GLFWwindow* window() const noexcept;
  Platform& platform() noexcept;
  [[nodiscard]] const Platform& platform() const noexcept;
  [[nodiscard]] const RenderSessionWindowConfig& windowConfig() const noexcept;
  [[nodiscard]] const RenderSessionConfig& sessionConfig() const noexcept;

  virtual SurfaceTextures createSurfaceTextures() noexcept = 0;
  virtual std::shared_ptr<Platform> createPlatform() noexcept = 0;

  virtual void willCreateWindow() noexcept {}
  virtual void didCreateWindow() noexcept {}

  virtual void willTick() noexcept {}

 private:
  bool createWindow() noexcept;

  std::shared_ptr<Platform> platform_;
  ShellParams shellParams_;
  RenderSessionWindowConfig windowConfig_;
  RenderSessionConfig sessionConfig_;
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window_;
  std::unique_ptr<RenderSession> session_;
};

} // namespace igl::shell
