/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <igl/vulkan/Common.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

#include <shell/openxr/impl/XrAppImpl.h>

namespace igl::shell::openxr {
class XrSwapchainProvider;
} // namespace igl::shell::openxr

namespace igl::shell::openxr::desktop {

class XrAppImplVulkan final : public impl::XrAppImpl {
 public:
  std::vector<const char*> getXrRequiredExtensions() const override;
  std::unique_ptr<igl::IDevice> initIGL(XrInstance instance, XrSystemId systemId) override;
  XrSession initXrSession(XrInstance instance, XrSystemId systemId, igl::IDevice& device) override;
  std::unique_ptr<impl::XrSwapchainProviderImpl> createSwapchainProviderImpl() const override;

 private:
  std::vector<const char*> processExtensionsBuffer(std::vector<char>& buffer);

 private:
  XrGraphicsRequirementsVulkanKHR graphicsRequirements_ = {
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
  };

  std::vector<const char*> requiredVkInstanceExtensions_;
  std::vector<char> requiredVkInstanceExtensionsBuffer_;

  std::vector<const char*> requiredVkDeviceExtensions_;
  std::vector<char> requiredVkDeviceExtensionsBuffer_;
};
} // namespace igl::shell::openxr::desktop
