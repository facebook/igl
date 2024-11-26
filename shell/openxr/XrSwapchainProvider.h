/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/openxr/XrPlatform.h>
#include <shell/openxr/impl/XrSwapchainProviderImpl.h>
#include <shell/shared/platform/Platform.h>

#include <igl/Device.h>
#include <igl/Texture.h>

// forward declarations
namespace igl::shell::openxr::impl {} // namespace igl::shell::openxr::impl

namespace igl::shell::openxr {

class XrSwapchainProvider {
 public:
  XrSwapchainProvider(std::unique_ptr<impl::XrSwapchainProviderImpl>&& impl,
                      std::shared_ptr<igl::shell::Platform> platform,
                      XrSession session,
                      impl::SwapchainImageInfo swapchainImageInfo,
                      uint8_t numViews) noexcept;
  ~XrSwapchainProvider() noexcept;

  [[nodiscard]] bool initialize() noexcept;

  [[nodiscard]] inline uint32_t currentImageIndex() const noexcept {
    return currentImageIndex_;
  }

  [[nodiscard]] igl::SurfaceTextures getSurfaceTextures() const noexcept;

  void releaseSwapchainImages() const noexcept;

  [[nodiscard]] inline XrSwapchain colorSwapchain() const noexcept {
    return colorSwapchain_;
  }

  [[nodiscard]] inline XrSwapchain depthSwapchain() const noexcept {
    return depthSwapchain_;
  }

 private:
  XrSwapchain createXrSwapchain(XrSwapchainUsageFlags extraUsageFlags, int64_t format) noexcept;

  std::unique_ptr<impl::XrSwapchainProviderImpl> impl_;
  std::shared_ptr<igl::shell::Platform> platform_;
  const XrSession session_;
  impl::SwapchainImageInfo swapchainImageInfo_;

  XrSwapchain colorSwapchain_ = XR_NULL_HANDLE;
  XrSwapchain depthSwapchain_ = XR_NULL_HANDLE;
  uint32_t currentImageIndex_ = 0;

  const uint8_t numViews_ =
      1; // The number of layers of the underlying swapchain image would match numViews_.
};
} // namespace igl::shell::openxr
