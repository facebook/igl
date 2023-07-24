/*
* LightweightVK
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// clang-format off
#ifdef _WIN32
#  define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#else
#  error Unsupported OS
#endif
// clang-format on

#include <GLFW/glfw3native.h>

#include <lvk/LVK.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

namespace lvk {

static GLFWwindow* initWindow(const char* windowTitle, uint32_t& outWidth, uint32_t& outHeight, bool resizable = false) {
  if (!glfwInit()) {
    return nullptr;
  }

  const bool wantsWholeArea = !outWidth || !outHeight;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, wantsWholeArea || !resizable ? GLFW_FALSE : GLFW_TRUE);

  // render full screen without overlapping taskbar
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();

  int x = 0;
  int y = 0;
  int w = (int)outWidth;
  int h = (int)outHeight;

  if (wantsWholeArea) {
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
  }

  GLFWwindow* window = glfwCreateWindow(w, h, windowTitle, nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return nullptr;
  }

  if (wantsWholeArea) {
    glfwSetWindowPos(window, x, y);
  }

  glfwGetWindowSize(window, &w, &h);

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  });

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  outWidth = (uint32_t)w;
  outHeight = (uint32_t)h;

  return window;
}

static std::unique_ptr<lvk::IDevice> createVulkanDeviceWithSwapchain(
    GLFWwindow* window,
    uint32_t width,
    uint32_t height,
    const lvk::vulkan::VulkanContextConfig& cfg,
    lvk::HWDeviceType preferredDeviceType = lvk::HWDeviceType::DiscreteGpu) {
   using namespace lvk;
#if defined(_WIN32)
  auto ctx = vulkan::Device::createContext(cfg, (void*)glfwGetWin32Window(window));
#elif defined(__linux__)
  auto ctx = vulkan::Device::createContext(cfg, (void*)glfwGetX11Window(window), (void*)glfwGetX11Display());
#else
#error Unsupported OS
#endif

  std::vector<HWDeviceDesc> devices =
      vulkan::Device::queryDevices(*ctx.get(), preferredDeviceType, nullptr);
  
  if (devices.empty()) {
    if (preferredDeviceType == HWDeviceType::DiscreteGpu) {
      devices = vulkan::Device::queryDevices(*ctx.get(), HWDeviceType::IntegratedGpu, nullptr);
    }
    if (preferredDeviceType == HWDeviceType::IntegratedGpu) {
      devices = vulkan::Device::queryDevices(*ctx.get(), HWDeviceType::DiscreteGpu, nullptr);
    }
  }

  if (devices.empty()) {
    IGL_ASSERT_MSG(false, "GPU is not found");
    return nullptr;
  }

  auto device = vulkan::Device::create(std::move(ctx), devices[0], width, height);
  IGL_ASSERT(device.get());

  return device;
}

} // namespace lvk
