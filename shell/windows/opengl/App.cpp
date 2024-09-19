/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/Macros.h>

#if defined(FORCE_USE_ANGLE) || IGL_PLATFORM_LINUX
#include <igl/opengl/egl/HWDevice.h>
#else
#include <igl/opengl/wgl/HWDevice.h>
#endif // FORCE_USE_ANGLE

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

// clang-format on

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "AutoContextReleaseDevice.h"
#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/Version.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/DefaultRenderSessionFactory.h>
#include <shell/shared/renderSession/IRenderSessionFactory.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/shared/renderSession/transition/TransitionRenderSessionFactory.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>

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

GLFWwindow* initGLWindow(const shell::RenderSessionConfig& config) {
  glfwSetErrorCallback(glfwErrorHandler);
  if (!glfwInit()) {
    IGLLog(IGLLogError, "initGLWindow> glfwInit failed");
    return nullptr;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.backendVersion.majorVersion);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.backendVersion.minorVersion);
  if (config.backendVersion.majorVersion > 3 ||
      (config.backendVersion.majorVersion == 3 && config.backendVersion.minorVersion >= 2)) {
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }
  glfwWindowHint(GLFW_VISIBLE, true);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_RESIZABLE, true);
  glfwWindowHint(GLFW_SRGB_CAPABLE, true);
  glfwWindowHint(GLFW_MAXIMIZED, config.screenMode != shell::ScreenMode::Windowed);

  GLFWwindow* windowHandle =
      glfwCreateWindow(config.width, config.height, config.displayName.c_str(), NULL, NULL);
  if (!windowHandle) {
    IGLLog(IGLLogError, "initGLWindow> we couldn't create the window");
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
                       if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                         glfwSetWindowShouldClose(window, GLFW_TRUE);
                       }
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
static void run(uint32_t majorVersion,
                uint32_t minorVersion,
                std::unique_ptr<igl::shell::IRenderSessionFactory> factory,
                const shell::RenderSessionConfig& config) {
  shellParams_ = initShellParams();
  using WindowPtr = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>;
  WindowPtr glWindow(initGLWindow(config), &glfwDestroyWindow);

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
    glSession_ = factory->createRenderSession(glShellPlatform_);
    IGL_ASSERT_MSG(glSession_,
                   "IRenderSessionFactory::createRenderSession() must return a valid session");
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

  auto factory = igl::shell::createDefaultRenderSessionFactory();

  uint32_t majorVersion = 4;
  uint32_t minorVersion = 6;
  if (argc == 2) {
    std::tie(majorVersion, minorVersion) = igl::opengl::parseVersionString(argv[1]);
  }

  std::vector<shell::RenderSessionConfig> suggestedConfigs = {
      {
          .displayName =
              "OpenGL " + std::to_string(majorVersion) + "." + std::to_string(minorVersion),
          .backendVersion = {.flavor = BackendFlavor::OpenGL,
                             .majorVersion = static_cast<uint8_t>(majorVersion),
                             .minorVersion = static_cast<uint8_t>(minorVersion)},
          .colorFramebufferFormat = TextureFormat::RGBA_SRGB,
          .width = 1024,
          .height = 768,
          .screenMode = shell::ScreenMode::Windowed,
      },
  };

  const auto requestedConfigs = factory->requestedConfigs(std::move(suggestedConfigs));
  if (IGL_UNEXPECTED(requestedConfigs.size() != 1)) {
    return -1;
  }

  IGL_ASSERT(requestedConfigs[0].backendVersion.flavor == BackendFlavor::OpenGL);

  run(majorVersion, minorVersion, std::move(factory), requestedConfigs[0]);

  return 0;
}
