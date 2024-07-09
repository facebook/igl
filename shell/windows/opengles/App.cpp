/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/Version.h>
#if IGL_ANGLE
#include <igl/opengl/egl/Context.h>
#include <igl/opengl/egl/Device.h>
#include <igl/opengl/egl/PlatformDevice.h>
#endif // IGL_ANGLE
#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif // _WIN32

#if IGL_ANGLE
#define GLFW_EXPOSE_NATIVE_EGL
#endif // IGL_ANGLE
#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace igl;

namespace {

// Backend toggle, currentonly Angle vs Vulkan
bool angleBackend = true;

void throwOnBadResult(const Result& result) {
  if (result.code != Result::Code::Ok) {
    std::stringstream errorMsg;
    errorMsg << "IGL error:\nCode: " << static_cast<int>(result.code)
             << "\nMessage: " << result.message;
    IGL_LOG_ERROR("%s", errorMsg.str().c_str());
    throw std::runtime_error(errorMsg.str());
  }
}

std::shared_ptr<igl::shell::Platform> glesShellPlatform_;
std::unique_ptr<igl::shell::RenderSession> glesSession_;
igl::shell::ShellParams shellParams_;

void glfwErrorHandler(int error, const char* description) {
  IGL_LOG_ERROR("GLFW Error: %s\n", description);
}

igl::shell::MouseButton getIGLMouseButton(int button) {
  if (button == GLFW_MOUSE_BUTTON_LEFT)
    return igl::shell::MouseButton::Left;

  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    return igl::shell::MouseButton::Right;

  return igl::shell::MouseButton::Middle;
}

} // namespace

class EGLDevice final : public ::igl::opengl::Device {
 public:
  explicit EGLDevice(std::unique_ptr<::igl::opengl::IContext> context) :
    Device(std::move(context)), platformDevice_(*this) {
    {}
  }

  const igl::opengl::PlatformDevice& getPlatformDevice() const noexcept override {
    return platformDevice_;
  }

  ::igl::opengl::PlatformDevice platformDevice_;
};

GLFWwindow* initGLESWindow(uint32_t majorVersion, uint32_t minorVersion) {
  glfwSetErrorCallback(glfwErrorHandler);
  if (!glfwInit())
    return nullptr;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, majorVersion);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minorVersion);
  glfwWindowHint(GLFW_VISIBLE, true);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_RESIZABLE, true);
  glfwWindowHint(GLFW_SRGB_CAPABLE, true);

  GLFWwindow* windowHandle = glfwCreateWindow(
      shellParams_.viewportSize.x, shellParams_.viewportSize.y, "Hello igl", NULL, NULL);
  if (!windowHandle) {
    glfwTerminate();
    return nullptr;
  }

  int result = glfwGetWindowAttrib(windowHandle, GLFW_CLIENT_API);

  glfwMakeContextCurrent(windowHandle);
  glfwSwapInterval(1);

  glfwSetCursorPosCallback(windowHandle, [](GLFWwindow* window, double xpos, double ypos) {
    glesShellPlatform_->getInputDispatcher().queueEvent(
        igl::shell::MouseMotionEvent((float)xpos, (float)ypos, 0, 0));
  });
  glfwSetScrollCallback(windowHandle, [](GLFWwindow* window, double xoffset, double yoffset) {
    glesShellPlatform_->getInputDispatcher().queueEvent(
        igl::shell::MouseWheelEvent((float)xoffset, (float)yoffset));
  });
  glfwSetMouseButtonCallback(
      windowHandle, [](GLFWwindow* window, int button, int action, int mods) {
        igl::shell::MouseButton iglButton = getIGLMouseButton(button);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        glesShellPlatform_->getInputDispatcher().queueEvent(igl::shell::MouseButtonEvent(
            iglButton, action == GLFW_PRESS, (float)xpos, (float)ypos));
      });

  glfwSetKeyCallback(windowHandle,
                     [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                       if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE) {
                         angleBackend = !angleBackend;
                         IGL_LOG_INFO("%s\n", angleBackend ? "Angle" : "Vulkan");
                       }
                     });

  IGL_LOG_INFO("Renderer: %s\n", (const char*)glGetString(GL_RENDERER));
  IGL_LOG_INFO("Version: %s\n", (const char*)glGetString(GL_VERSION));
  IGL_LOG_INFO("WindowAttrib: 0x%x\n", result);

  return windowHandle;
}

igl::SurfaceTextures createSurfaceTextures(igl::IDevice& device) {
#if IGL_ANGLE
  if (IGL_VERIFY(device.getBackendType() == igl::BackendType::OpenGL)) {
    auto platformDevice = device.getPlatformDevice<igl::opengl::egl::PlatformDevice>();
    IGL_ASSERT(platformDevice != nullptr);
    if (IGL_VERIFY(platformDevice)) {
      auto color = platformDevice->createTextureFromNativeDrawable(nullptr);
      auto depth =
          platformDevice->createTextureFromNativeDepth(igl::TextureFormat::Z_UNorm24, nullptr);
      return igl::SurfaceTextures{std::move(color), std::move(depth)};
    }
  }
#endif // IGL_ANGLE
  return igl::SurfaceTextures{};
}

int main(int argc, char* argv[]) {
  igl::shell::Platform::initializeCommandLineArgs(argc, argv);

  uint32_t majorVersion = 3;
  uint32_t minorVersion = 1;
  if (argc == 2) {
    std::tie(majorVersion, minorVersion) = igl::opengl::parseVersionString(argv[1]);
  }

  using WindowPtr = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>;
  WindowPtr glesWindow(initGLESWindow(majorVersion, minorVersion), &glfwDestroyWindow);
  if (!glesWindow.get())
    return 0;

#if IGL_ANGLE
  auto glesDevice = std::make_unique</*EGLDevice*/ igl::opengl::egl::Device>(
      std::make_unique<::igl::opengl::egl::Context>(glfwGetEGLDisplay(),
                                                    glfwGetEGLContext(glesWindow.get()),
                                                    glfwGetEGLSurface(glesWindow.get()),
                                                    glfwGetEGLSurface(glesWindow.get())));

  glesShellPlatform_ = std::make_shared<igl::shell::PlatformWin>(std::move(glesDevice));
#endif // IGL_ANGLE
  IGL_ASSERT(glesShellPlatform_);

  glesSession_ = igl::shell::createDefaultRenderSession(glesShellPlatform_);
  IGL_ASSERT_MSG(glesSession_, "createDefaultRenderSession() must return a valid session");
  glesSession_->initialize();

  auto surfaceTextures = createSurfaceTextures(glesShellPlatform_->getDevice());
  IGL_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);

  while (!glfwWindowShouldClose(glesWindow.get()) && !glesSession_->appParams().exitRequested) {
    glesShellPlatform_->getInputDispatcher().processEvents();
    glesSession_->update(surfaceTextures);
    glfwMakeContextCurrent(glesWindow.get());
    glfwSwapBuffers(glesWindow.get());
    glfwPollEvents();
  }

  glfwDestroyWindow(glesWindow.get());
  glfwTerminate();

  return 0;
}
