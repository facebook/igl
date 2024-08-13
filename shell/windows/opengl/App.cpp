/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// clang-format off
#if defined(_WIN32)
  #define GLFW_EXPOSE_NATIVE_WIN32
  #define GLFW_EXPOSE_NATIVE_WGL
#else
  #define GLFW_EXPOSE_NATIVE_X11
  #define GLFW_EXPOSE_NATIVE_GLX
#endif
#define GLFW_INCLUDE_NONE

#if !defined(IGL_CMAKE_BUILD)
  #if !defined(GLEW_STATIC)
    #define GLEW_STATIC
  #endif
#endif // IGL_CMAKE_BUILD
#if defined(_WIN32)
  #include <GL/glew.h>
#endif // _WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/Version.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include "AutoContextReleaseDevice.h"
#include "ImageTestApp.h"
// clang-format on

using namespace igl;

namespace {

// Backend toggle, currentonly Angle vs Vulkan
bool angleBackend = true;

igl::shell::ShellParams initShellParams() {
  igl::shell::ShellParams shellParams;
  shellParams.viewportSize = glm::vec2(1024.0f, 768.0f);
  shellParams.nativeSurfaceDimensions = glm::vec2(1024.0f, 768.0f);
  return shellParams;
}

void throwOnBadResult(const Result& result) {
  if (result.code != Result::Code::Ok) {
    std::stringstream errorMsg;
    errorMsg << "IGL error:\nCode: " << static_cast<int>(result.code)
             << "\nMessage: " << result.message;
    IGL_LOG_ERROR("%s", errorMsg.str().c_str());
    throw std::runtime_error(errorMsg.str());
  }
}

std::shared_ptr<igl::shell::Platform> glShellPlatform_;
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

igl::SurfaceTextures createSurfaceTextures(igl::IDevice& device) {
  if (IGL_VERIFY(device.getBackendType() == igl::BackendType::OpenGL)) {
    igl::opengl::Device& oglDevice = static_cast<igl::opengl::Device&>(device);
    oglDevice.getContext().setCurrent();
    TextureDesc desc = {
        static_cast<uint32_t>(shellParams_.viewportSize.x),
        static_cast<uint32_t>(shellParams_.viewportSize.y),
        1,
        1,
        1,
        TextureDesc::TextureUsageBits::Attachment,
        1,
        TextureType::TwoD,
        shellParams_.defaultColorFramebufferFormat,
    };
    auto color =
        std::make_shared<igl::opengl::ViewTextureTarget>(oglDevice.getContext(), desc.format);
    color->create(desc, true);
    desc.format = TextureFormat::Z_UNorm24;
    auto depth =
        std::make_shared<igl::opengl::ViewTextureTarget>(oglDevice.getContext(), desc.format);
    depth->create(desc, true);
    return igl::SurfaceTextures{std::move(color), std::move(depth)};
  }

  return igl::SurfaceTextures{};
}

} // namespace

GLFWwindow* initGLWindow(uint32_t majorVersion, uint32_t minorVersion) {
  glfwSetErrorCallback(glfwErrorHandler);
  if (!glfwInit()) {
    IGLLog(IGLLogLevel::LOG_ERROR, "initGLWindow> glfwInit failed");
    return nullptr;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, majorVersion);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minorVersion);
  if (majorVersion > 3 || (majorVersion == 3 && minorVersion >= 2)) {
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }
  glfwWindowHint(GLFW_VISIBLE, true);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_RESIZABLE, true);
  glfwWindowHint(GLFW_SRGB_CAPABLE, true);

  GLFWwindow* windowHandle = glfwCreateWindow(
      shellParams_.viewportSize.x, shellParams_.viewportSize.y, "Hello igl", NULL, NULL);
  if (!windowHandle) {
    IGLLog(IGLLogLevel::LOG_ERROR, "initGLWindow> we couldn't create the window");
    glfwTerminate();
    return nullptr;
  }

  int result = glfwGetWindowAttrib(windowHandle, GLFW_CLIENT_API);

  glfwMakeContextCurrent(windowHandle);

#if defined(_WIN32)
  glewExperimental = GL_TRUE;
  glewInit();
#endif

  glfwSwapInterval(1);

  glfwSetCursorPosCallback(windowHandle, [](GLFWwindow* window, double xpos, double ypos) {
    glShellPlatform_->getInputDispatcher().queueEvent(
        igl::shell::MouseMotionEvent((float)xpos, (float)ypos, 0, 0));
  });
  glfwSetScrollCallback(windowHandle, [](GLFWwindow* window, double xoffset, double yoffset) {
    glShellPlatform_->getInputDispatcher().queueEvent(
        igl::shell::MouseWheelEvent((float)xoffset, (float)yoffset));
  });
  glfwSetMouseButtonCallback(
      windowHandle, [](GLFWwindow* window, int button, int action, int mods) {
        igl::shell::MouseButton iglButton = getIGLMouseButton(button);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        glShellPlatform_->getInputDispatcher().queueEvent(igl::shell::MouseButtonEvent(
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

// This mode is the normal running mode when running samples as applications.
static void RunApplicationMode(uint32_t majorVersion, uint32_t minorVersion) {
  shellParams_ = initShellParams();
  using WindowPtr = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>;
  WindowPtr glWindow(initGLWindow(majorVersion, minorVersion), &glfwDestroyWindow);

  if (!glWindow.get())
    return;

#if defined(_WIN32)
  auto context = std::make_unique<igl::opengl::wgl::Context>(
      GetDC(glfwGetWin32Window(glWindow.get())), glfwGetWGLContext(glWindow.get()));
  auto glDevice = std::make_unique<shell::util::WGLDevice>(std::move(context));

  glShellPlatform_ = std::make_shared<igl::shell::PlatformWin>(std::move(glDevice));
#elif IGL_PLATFORM_LINUX
  auto context = std::make_unique<igl::opengl::glx::Context>(
      nullptr,
      glfwGetX11Display(),
      (igl::opengl::glx::GLXDrawable)glfwGetX11Window(glWindow.get()),
      (igl::opengl::glx::GLXContext)glfwGetGLXContext(glWindow.get()));

  auto glDevice = std::make_unique<igl::opengl::glx::Device>(std::move(context));

  glShellPlatform_ = std::make_shared<igl::shell::PlatformWin>(std::move(glDevice));
#endif
  {
    std::unique_ptr<igl::shell::RenderSession> glSession_;
    glSession_ = igl::shell::createDefaultRenderSession(glShellPlatform_);
    IGL_ASSERT_MSG(glSession_, "createDefaultRenderSession() must return a valid session");
    glSession_->setShellParams(shellParams_);
    glSession_->initialize();

    auto surfaceTextures = createSurfaceTextures(glShellPlatform_->getDevice());
    IGL_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);

    glfwSwapInterval(0);

    while (!glfwWindowShouldClose(glWindow.get()) && !glSession_->appParams().exitRequested) {
#if defined(_WIN32)
      static_cast<shell::util::WGLDevice&>(glShellPlatform_->getDevice()).getContext().setCurrent();
#elif IGL_PLATFORM_LINUX
      static_cast<shell::util::GLXDevice&>(glShellPlatform_->getDevice()).getContext().setCurrent();
#endif
      glShellPlatform_->getInputDispatcher().processEvents();
      glSession_->update(surfaceTextures);
      glfwPollEvents();
    }
  }

  glShellPlatform_ = nullptr;
  glDevice = nullptr;
  context = nullptr;
  glWindow = nullptr;

  glfwTerminate();
}

int main(int argc, char* argv[]) {
  igl::shell::Platform::initializeCommandLineArgs(argc, argv);

  uint32_t majorVersion = 4;
  uint32_t minorVersion = 6;
  if (argc == 2) {
    std::tie(majorVersion, minorVersion) = igl::opengl::parseVersionString(argv[1]);
  }

  const char* screenshotTestsOutPath = std::getenv("SCREENSHOT_TESTS_OUT");

  if (screenshotTestsOutPath) {
    shell::util::RunScreenshotTestsMode(shellParams_);
  } else {
    RunApplicationMode(majorVersion, minorVersion);
  }

  return 0;
}
