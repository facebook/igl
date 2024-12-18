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
#if !defined(IGL_CMAKE_BUILD)
  #if !defined(GLEW_STATIC)
    #define GLEW_STATIC
  #endif
#endif // IGL_CMAKE_BUILD
#if defined(_WIN32)
  #include <GL/glew.h>
#endif // _WIN32

// clang-format on
#include "AutoContextReleaseDevice.h"
#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/Version.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/windows/common/GlfwShell.h>

using namespace igl;
namespace igl::shell {
namespace {
class OpenGlShell final : public GlfwShell {
  igl::SurfaceTextures createSurfaceTextures() noexcept final;
  std::shared_ptr<Platform> createPlatform() noexcept final;

  void willCreateWindow() noexcept final;
  void didCreateWindow() noexcept final;

  void willTick() noexcept final;
};

igl::SurfaceTextures OpenGlShell::createSurfaceTextures() noexcept {
  auto& device = platform().getDevice();
  if (IGL_DEBUG_VERIFY(device.getBackendType() == igl::BackendType::OpenGL)) {
    igl::opengl::Device& oglDevice = static_cast<igl::opengl::Device&>(device);
    oglDevice.getContext().setCurrent();
    TextureDesc desc = {
        static_cast<uint32_t>(shellParams().viewportSize.x),
        static_cast<uint32_t>(shellParams().viewportSize.y),
        1,
        1,
        1,
        TextureDesc::TextureUsageBits::Attachment,
        1,
        TextureType::TwoD,
        sessionConfig().swapchainColorTextureFormat,
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

void OpenGlShell::willCreateWindow() noexcept {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, sessionConfig().backendVersion.majorVersion);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, sessionConfig().backendVersion.minorVersion);
  if (sessionConfig().backendVersion.majorVersion > 3 ||
      (sessionConfig().backendVersion.majorVersion == 3 &&
       sessionConfig().backendVersion.minorVersion >= 2)) {
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
}

void OpenGlShell::didCreateWindow() noexcept {
  [[maybe_unused]] int result = glfwGetWindowAttrib(&window(), GLFW_CLIENT_API);

  glfwMakeContextCurrent(&window());

#if defined(_WIN32)
  glewExperimental = GL_TRUE;
  glewInit();
#endif

  glfwSwapInterval(1);

  IGL_LOG_INFO("Renderer: %s\n", (const char*)glGetString(GL_RENDERER));
  IGL_LOG_INFO("Version: %s\n", (const char*)glGetString(GL_VERSION));
  IGL_LOG_INFO("WindowAttrib: 0x%x\n", result);
}

std::shared_ptr<Platform> OpenGlShell::createPlatform() noexcept {
#if defined(_WIN32)
  auto context = std::make_unique<igl::opengl::wgl::Context>(GetDC(glfwGetWin32Window(&window())),
                                                             glfwGetWGLContext(&window()));
  auto glDevice = std::make_unique<shell::util::WGLDevice>(std::move(context));

  return std::make_shared<igl::shell::PlatformWin>(std::move(glDevice));
#elif IGL_PLATFORM_LINUX
  auto context = std::make_unique<igl::opengl::glx::Context>(
      nullptr,
      glfwGetX11Display(),
      (igl::opengl::glx::GLXDrawable)glfwGetX11Window(&window()),
      (igl::opengl::glx::GLXContext)glfwGetGLXContext(&window()));

  auto glDevice = std::make_unique<igl::opengl::glx::Device>(std::move(context));

  return std::make_shared<igl::shell::PlatformWin>(std::move(glDevice));
#endif
}

void OpenGlShell::willTick() noexcept {
#if defined(_WIN32)
  static_cast<shell::util::WGLDevice&>(platform().getDevice()).getContext().setCurrent();
#elif IGL_PLATFORM_LINUX
  static_cast<shell::util::GLXDevice&>(platform().getDevice()).getContext().setCurrent();
#endif
}

} // namespace
} // namespace igl::shell

int main(int argc, char* argv[]) {
  igl::shell::OpenGlShell shell;

  uint32_t majorVersion = 4;
  uint32_t minorVersion = 6;
  if (argc == 2) {
    std::tie(majorVersion, minorVersion) = igl::opengl::parseVersionString(argv[1]);
  }

  igl::shell::RenderSessionWindowConfig suggestedWindowConfig = {
      .width = 1024,
      .height = 768,
      .windowMode = shell::WindowMode::Window,
  };
  igl::shell::RenderSessionConfig suggestedConfig = {
      .displayName = "OpenGL " + std::to_string(majorVersion) + "." + std::to_string(minorVersion),
      .backendVersion = {.flavor = BackendFlavor::OpenGL,
                         .majorVersion = static_cast<uint8_t>(majorVersion),
                         .minorVersion = static_cast<uint8_t>(minorVersion)},
      .swapchainColorTextureFormat = TextureFormat::RGBA_SRGB,
  };

  if (!shell.initialize(argc, argv, suggestedWindowConfig, std::move(suggestedConfig))) {
    shell.teardown();
    return -1;
  }

  shell.run();
  shell.teardown();

  return 0;
}
