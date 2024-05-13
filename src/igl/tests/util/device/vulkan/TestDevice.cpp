/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/vulkan/TestDevice.h>

#include <igl/Common.h>
#include <igl/Macros.h>

#if IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOS || IGL_PLATFORM_LINUX
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#else
#error "Unsupported testing platform"
#endif

#if IGL_PLATFORM_MACOS
#include <igl/vulkan/moltenvk/MoltenVKHelpers.h>
#endif

namespace igl::tests::util::device::vulkan {

//
// createTestDevice
//
// Used by clients to get an IGL device.
//
std::shared_ptr<::igl::IDevice> createTestDevice(bool enableValidation) {
#if IGL_PLATFORM_MACOS
  ::igl::vulkan::setupMoltenVKEnvironment();
#endif

  std::shared_ptr<igl::IDevice> iglDev = nullptr;
  Result ret;

  igl::vulkan::VulkanContextConfig config;
  config.enhancedShaderDebugging = false; // This causes issues for MoltenVK
  config.enableValidation = enableValidation;
  config.enableGPUAssistedValidation = enableValidation;

#if IGL_PLATFORM_MACOS
  config.terminateOnValidationError = false;
#elif IGL_DEBUG
  config.terminateOnValidationError = enableValidation;
#else
  config.enableValidation = false;
  config.terminateOnValidationError = false;
#endif
#ifdef IGL_DISABLE_VALIDATION
  config.enableValidation = false;
  config.terminateOnValidationError = false;
#endif
  // Disables OS Level Color Management to achieve parity with OpenGL
  config.swapChainColorSpace = igl::ColorSpace::PASS_THROUGH;
  config.enableExtraLogs = enableValidation;

  auto ctx = igl::vulkan::HWDevice::createContext(config, nullptr);

  std::vector<HWDeviceDesc> devices = igl::vulkan::HWDevice::queryDevices(
      *ctx.get(), HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);

  if (ret.isOk()) {
    std::vector<const char*> extraDeviceExtensions;
    extraDeviceExtensions.emplace_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

    iglDev = igl::vulkan::HWDevice::create(std::move(ctx),
                                           devices[0],
                                           0, // width
                                           0, // height,
                                           extraDeviceExtensions.size(),
                                           extraDeviceExtensions.data(),
                                           &ret);

    if (!ret.isOk()) {
      iglDev = nullptr;
    }
  }

  return iglDev;
}

} // namespace igl::tests::util::device::vulkan
