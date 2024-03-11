/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>

#include <android/native_window_jni.h>

#define VK_USE_PLATFORM_ANDROID_KHR
#include <igl/vulkan/Common.h>

#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include <openxr/openxr_platform.h>

#include <shell/openxr/impl/XrAppImpl.h>

namespace igl::shell::openxr::mobile {
class XrSwapchainProvider;
class XrAppImplVulkan final : public impl::XrAppImpl {
 public:
  std::vector<const char*> getXrRequiredExtensions() const override;
  void* getInstanceCreateExtension() override;
  std::unique_ptr<igl::IDevice> initIGL(XrInstance instance, XrSystemId systemId) override;
  XrSession initXrSession(XrInstance instance, XrSystemId systemId, igl::IDevice& device) override;
  std::unique_ptr<impl::XrSwapchainProviderImpl> createSwapchainProviderImpl() const override;

 private:
  std::vector<const char*> processExtensionsBuffer(std::vector<char>& buffer);

 private:
  XrGraphicsRequirementsVulkanKHR graphicsRequirements_ = {
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
  };
#if defined(IGL_CMAKE_BUILD)
  XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid_ = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
  };
#endif // IGL_CMAKE_BUILD

  XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid_ = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
  };

  std::vector<const char*> requiredVkInstanceExtensions_;
  std::vector<char> requiredVkInstanceExtensionsBuffer_;

  std::vector<const char*> requiredVkDeviceExtensions_;
  std::vector<char> requiredVkDeviceExtensionsBuffer_;
};
} // namespace igl::shell::openxr::mobile
