/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <android/native_window_jni.h>

#define VK_USE_PLATFORM_ANDROID_KHR
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanTexture.h>

#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#include <openxr/openxr_platform.h>

#include <shell/openxr/impl/XrSwapchainProviderImpl.h>

namespace igl::shell::openxr::mobile {
class XrSwapchainProviderImplVulkan final : public impl::XrSwapchainProviderImpl {
 public:
  int64_t preferredColorFormat() const final {
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
  int64_t preferredDepthFormat() const final {
    return VK_FORMAT_D24_UNORM_S8_UINT;
  }
  void enumerateImages(igl::IDevice& device,
                       XrSwapchain colorSwapchain,
                       XrSwapchain depthSwapchain,
                       int64_t selectedColorFormat,
                       int64_t selectedDepthFormat,
                       const XrViewConfigurationView& viewport,
                       uint32_t numViews) final;
  igl::SurfaceTextures getSurfaceTextures(igl::IDevice& device,
                                          const XrSwapchain& colorSwapchain,
                                          const XrSwapchain& depthSwapchain,
                                          int64_t selectedColorFormat,
                                          int64_t selectedDepthFormat,
                                          const XrViewConfigurationView& viewport,
                                          uint32_t numViews) final;

 private:
  std::vector<std::shared_ptr<igl::vulkan::VulkanTexture>> vulkanColorTextures_;
  std::vector<std::shared_ptr<igl::vulkan::VulkanTexture>> vulkanDepthTextures_;
};
} // namespace igl::shell::openxr::mobile
