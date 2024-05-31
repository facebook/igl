/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>

#include <shell/openxr/impl/XrAppImpl.h>

#include <vector>

namespace igl::shell::openxr::mobile {
class XrSwapchainProvider;
class XrAppImplVulkan : public impl::XrAppImpl {
 public:
  [[nodiscard]] std::vector<const char*> getXrRequiredExtensions() const override;
  [[nodiscard]] std::vector<const char*> getXrOptionalExtensions() const override;

  [[nodiscard]] std::unique_ptr<igl::IDevice> initIGL(XrInstance instance,
                                                      XrSystemId systemId) override;
  [[nodiscard]] XrSession initXrSession(XrInstance instance,
                                        XrSystemId systemId,
                                        igl::IDevice& device) override;
  [[nodiscard]] std::unique_ptr<impl::XrSwapchainProviderImpl> createSwapchainProviderImpl()
      const override;

 private:
  std::vector<const char*> processExtensionsBuffer(std::vector<char>& buffer);

  XrGraphicsRequirementsVulkanKHR graphicsRequirements_ = {
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
  };

  std::vector<const char*> requiredVkInstanceExtensions_;
  std::vector<char> requiredVkInstanceExtensionsBuffer_;

  std::vector<const char*> requiredVkDeviceExtensions_;
  std::vector<char> requiredVkDeviceExtensionsBuffer_;
};
} // namespace igl::shell::openxr::mobile
