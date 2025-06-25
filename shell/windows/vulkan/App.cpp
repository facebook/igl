/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/windows/common/GlfwShell.h>
#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>

using namespace igl;
namespace igl::shell {
namespace {
class VulkanShell final : public GlfwShell {
  SurfaceTextures createSurfaceTextures() noexcept final;
  std::shared_ptr<Platform> createPlatform() noexcept final;

  void willCreateWindow() noexcept final;
};

void VulkanShell::willCreateWindow() noexcept {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

std::shared_ptr<Platform> VulkanShell::createPlatform() noexcept {
  igl::vulkan::VulkanContextConfig cfg = igl::vulkan::VulkanContextConfig();
  cfg.headless = shellParams().isHeadless;
  cfg.requestedSwapChainTextureFormat = sessionConfig().swapchainColorTextureFormat;
#if defined(_MSC_VER) && !IGL_DEBUG
  cfg.enableValidation = false;
#endif
  auto ctx =
      vulkan::HWDevice::createContext(cfg,
#if defined(_WIN32)
                                      window() ? (void*)glfwGetWin32Window(window()) : nullptr
#else
                                      window() ? (void*)glfwGetX11Window(window()) : nullptr,
                                      0,
                                      nullptr,
                                      (void*)glfwGetX11Display()
#endif
      );

  // Prioritize discrete GPUs. If not found, use any that is available.
  std::vector<HWDeviceDesc> devices =
      vulkan::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::DiscreteGpu), nullptr);
  if (devices.empty()) {
    devices = vulkan::HWDevice::queryDevices(
        *ctx, HWDeviceQueryDesc(HWDeviceType::IntegratedGpu), nullptr);
  }
  if (devices.empty()) {
    // Lavapipe etc
    devices =
        vulkan::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::SoftwareGpu), nullptr);
  }
  IGL_DEBUG_ASSERT(devices.size() > 0, "Could not find Vulkan device with requested capabilities");

  auto vulkanDevice = vulkan::HWDevice::create(std::move(ctx),
                                               devices[0],
                                               (uint32_t)shellParams().viewportSize.x,
                                               (uint32_t)shellParams().viewportSize.y);

  return std::make_shared<PlatformWin>(std::move(vulkanDevice));
}

SurfaceTextures VulkanShell::createSurfaceTextures() noexcept {
  IGL_PROFILER_FUNCTION();
  auto& device = platform().getDevice();
  const auto& vkPlatformDevice = device.getPlatformDevice<igl::vulkan::PlatformDevice>();

  IGL_DEBUG_ASSERT(vkPlatformDevice != nullptr);

  Result ret;
  auto color = vkPlatformDevice->createTextureFromNativeDrawable(&ret);
  IGL_DEBUG_ASSERT(ret.isOk());
  auto depth = vkPlatformDevice->createTextureFromNativeDepth(
      shellParams().viewportSize.x, shellParams().viewportSize.y, &ret);
  IGL_DEBUG_ASSERT(ret.isOk());

  return SurfaceTextures{std::move(color), std::move(depth)};
}
} // namespace

} // namespace igl::shell

int main(int argc, char* argv[]) {
  igl::shell::VulkanShell shell;

  igl::shell::RenderSessionWindowConfig suggestedWindowConfig = {
      .width = 1024,
      .height = 768,
      .windowMode = shell::WindowMode::MaximizedWindow,
  };
  igl::shell::RenderSessionConfig suggestedConfig = {
      .displayName = "Vulkan 1.1",
      .backendVersion = {.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 1},
      .swapchainColorTextureFormat = TextureFormat::BGRA_SRGB,
  };

  if (!shell.initialize(argc, argv, suggestedWindowConfig, suggestedConfig)) {
    shell.teardown();
    return -1;
  }

  shell.run();
  shell.teardown();

  return 0;
}
