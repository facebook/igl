/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif // _WIN32

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>

using namespace igl;

#define IGL_SAMPLE_LOG_INFO(...) IGL_LOG_INFO(__VA_ARGS__)
#define IGL_SAMPLE_LOG_ERROR(...) IGL_LOG_ERROR(__VA_ARGS__)

namespace {

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
    IGL_SAMPLE_LOG_ERROR("%s", errorMsg.str().c_str());
    throw std::runtime_error(errorMsg.str());
  }
}

std::shared_ptr<igl::shell::Platform> vulkanShellPlatform_;
std::unique_ptr<igl::shell::RenderSession> vulkanSession_;
igl::shell::ShellParams shellParams_;

void glfwErrorHandler(int error, const char* description) {
  IGL_SAMPLE_LOG_ERROR("GLFW Error (%i): %s\n", error, description);
}

igl::shell::MouseButton getIGLMouseButton(int button) {
  if (button == GLFW_MOUSE_BUTTON_LEFT)
    return igl::shell::MouseButton::Left;

  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    return igl::shell::MouseButton::Right;

  return igl::shell::MouseButton::Middle;
}

} // namespace

GLFWwindow* initWindow() {
  glfwSetErrorCallback(glfwErrorHandler);
  if (!glfwInit())
    return nullptr;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  const bool overrideDefaults = shellParams_.viewportSize == igl::shell::ShellParams().viewportSize;

  int x = 0;
  int y = 0;
  int w = shellParams_.viewportSize.x;
  int h = shellParams_.viewportSize.y;

  if (overrideDefaults) {
    // override defaults
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
  }

  GLFWwindow* windowHandle = glfwCreateWindow(w, h, "Hello IGL Vulkan", nullptr, nullptr);
  if (!windowHandle) {
    glfwTerminate();
    return nullptr;
  }

  int width, height;
  glfwGetFramebufferSize(windowHandle, &width, &height);
  shellParams_.viewportSize.x = width;
  shellParams_.viewportSize.y = height;

  if (overrideDefaults) {
    glfwSetWindowPos(windowHandle, x, y);
  }

  glfwSetCursorPosCallback(windowHandle, [](GLFWwindow* window, double xpos, double ypos) {
    vulkanShellPlatform_->getInputDispatcher().queueEvent(
        igl::shell::MouseMotionEvent((float)xpos, (float)ypos, 0, 0));
  });

  glfwSetScrollCallback(windowHandle, [](GLFWwindow* window, double xoffset, double yoffset) {
    vulkanShellPlatform_->getInputDispatcher().queueEvent(
        igl::shell::MouseWheelEvent((float)xoffset, (float)yoffset));
  });

  glfwSetMouseButtonCallback(
      windowHandle, [](GLFWwindow* window, int button, int action, int mods) {
        igl::shell::MouseButton iglButton = getIGLMouseButton(button);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        vulkanShellPlatform_->getInputDispatcher().queueEvent(igl::shell::MouseButtonEvent(
            iglButton, action == GLFW_PRESS, (float)xpos, (float)ypos));
      });

  glfwSetKeyCallback(windowHandle, [](GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  });

  return windowHandle;
}

std::shared_ptr<igl::shell::PlatformWin> createPlatform(GLFWwindow* window) {
  igl::vulkan::VulkanContextConfig cfg = igl::vulkan::VulkanContextConfig();
#if defined(_MSC_VER) && !IGL_DEBUG
  cfg.enableValidation = false;
#endif
  auto ctx = vulkan::HWDevice::createContext(cfg,
#if defined(_WIN32)
                                             (void*)glfwGetWin32Window(window)
#else
                                             (void*)glfwGetX11Window(window),
                                             0,
                                             nullptr,
                                             (void*)glfwGetX11Display()
#endif
  );

  // Prioritize discrete GPUs. If not found, use any that is available.
  std::vector<HWDeviceDesc> devices = vulkan::HWDevice::queryDevices(
      *ctx.get(), HWDeviceQueryDesc(HWDeviceType::DiscreteGpu), nullptr);
  if (devices.empty()) {
    devices = vulkan::HWDevice::queryDevices(
        *ctx.get(), HWDeviceQueryDesc(HWDeviceType::Unknown), nullptr);
  }
  IGL_ASSERT_MSG(devices.size() > 0, "Could not find Vulkan device with requested capabilities");

  auto vulkanDevice = vulkan::HWDevice::create(std::move(ctx),
                                               devices[0],
                                               (uint32_t)shellParams_.viewportSize.x,
                                               (uint32_t)shellParams_.viewportSize.y);

  return std::make_shared<igl::shell::PlatformWin>(std::move(vulkanDevice));
}

igl::SurfaceTextures getVulkanSurfaceTextures(igl::IDevice& device) {
  IGL_PROFILER_FUNCTION();

  const auto& vkPlatformDevice = device.getPlatformDevice<igl::vulkan::PlatformDevice>();

  IGL_ASSERT(vkPlatformDevice != nullptr);

  Result ret;
  auto color = vkPlatformDevice->createTextureFromNativeDrawable(&ret);
  IGL_ASSERT(ret.isOk());
  auto depth = vkPlatformDevice->createTextureFromNativeDepth(
      shellParams_.viewportSize.x, shellParams_.viewportSize.y, &ret);
  IGL_ASSERT(ret.isOk());

  return igl::SurfaceTextures{std::move(color), std::move(depth)};
}

int main(int argc, char* argv[]) {
  shellParams_ = initShellParams();
  igl::shell::Platform::initializeCommandLineArgs(argc, argv);

  using WindowPtr = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>;

  WindowPtr vulkanWindow(initWindow(), &glfwDestroyWindow);
  if (!vulkanWindow.get()) {
    return 0;
  }

  vulkanShellPlatform_ = createPlatform(vulkanWindow.get());
  vulkanSession_ = igl::shell::createDefaultRenderSession(vulkanShellPlatform_);

  IGL_ASSERT_MSG(vulkanSession_, "createDefaultRenderSession() must return a valid session");
  vulkanSession_->setShellParams(shellParams_);
  vulkanSession_->initialize();

  while (!glfwWindowShouldClose(vulkanWindow.get()) && !vulkanSession_->appParams().exitRequested) {
    auto surfaceTextures = getVulkanSurfaceTextures(vulkanShellPlatform_->getDevice());
    IGL_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);

    vulkanShellPlatform_->getInputDispatcher().processEvents();
    vulkanSession_->update(std::move(surfaceTextures));
    glfwPollEvents();
  }

  // Explicitly destroy all objects before exiting in order to make sure that
  // whatever else global destructors may there, will be called after these. One
  // example is a graphics resource tracker in the client code, which otherwise
  // would not be guaranteed to be called after the graphics resources release.
  vulkanSession_ = nullptr;
  vulkanShellPlatform_ = nullptr;
  vulkanWindow = nullptr;

  glfwTerminate();

  return 0;
}
