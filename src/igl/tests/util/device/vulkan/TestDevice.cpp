/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/vulkan/TestDevice.h>

#include <igl/Common.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_IOS || \
    IGL_PLATFORM_LINUX
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFeatures.h>
#endif

#if IGL_PLATFORM_MACOSX
#include <igl/vulkan/moltenvk/MoltenVKHelpers.h>
#endif

namespace igl::tests::util::device::vulkan {

//
// createTestDevice
//
// Used by clients to get an IGL device.
//

igl::vulkan::VulkanContextConfig getContextConfig(bool enableValidation) {
  igl::vulkan::VulkanContextConfig config;
  config.enhancedShaderDebugging = false; // This causes issues for MoltenVK
  config.enableValidation = enableValidation;
  config.enableGPUAssistedValidation = enableValidation;

#if IGL_PLATFORM_MACOSX
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
  config.swapChainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;
  config.enableExtraLogs = enableValidation;

  return config;
}

std::shared_ptr<IDevice> createTestDevice(const igl::vulkan::VulkanContextConfig& config) {
#if IGL_PLATFORM_MACOSX
  ::igl::vulkan::setupMoltenVKEnvironment();
#endif

  std::shared_ptr<IDevice> iglDev = nullptr;
  Result ret;

  auto ctx = igl::vulkan::HWDevice::createContext(config, nullptr);

  std::vector<HWDeviceDesc> devices =
      igl::vulkan::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);

  if (ret.isOk()) {
    std::vector<const char*> extraDeviceExtensions;
    extraDeviceExtensions.emplace_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

    igl::vulkan::VulkanFeatures features(config);
    features.populateWithAvailablePhysicalDeviceFeatures(*ctx, (VkPhysicalDevice)devices[0].guid);

    iglDev = igl::vulkan::HWDevice::create(std::move(ctx),
                                           devices[0],
                                           0, // width
                                           0, // height,
                                           extraDeviceExtensions.size(),
                                           extraDeviceExtensions.data(),
                                           &features,
                                           "Test Device",
                                           &ret);

    if (!ret.isOk()) {
      iglDev = nullptr;
    }
  }

  return iglDev;
}

std::shared_ptr<IDevice> createTestDevice(bool enableValidation) {
  return createTestDevice(getContextConfig(enableValidation));
}

} // namespace igl::tests::util::device::vulkan
