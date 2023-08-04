/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <memory>
#include <vector>

#include <openxr/openxr.h>

#include <igl/Device.h>
#include <igl/Texture.h>
#include <shell/shared/platform/Platform.h>

// forward declarations
namespace igl::shell::openxr::impl {
class XrSwapchainProviderImpl;
}

namespace igl::shell::openxr {

class XrSwapchainProvider {
 public:
  XrSwapchainProvider(std::unique_ptr<impl::XrSwapchainProviderImpl>&& impl,
                      const std::shared_ptr<igl::shell::Platform>& platform,
                      const XrSession& session,
                      const XrViewConfigurationView& viewport,
                      uint32_t numViews);
  ~XrSwapchainProvider();

  bool initialize();

  inline uint32_t currentImageIndex() const {
    return currentImageIndex_;
  }
  igl::SurfaceTextures getSurfaceTextures() const;
  void releaseSwapchainImages() const;

  inline XrSwapchain colorSwapchain() const {
    return colorSwapchain_;
  }

  inline XrSwapchain depthSwapchain() const {
    return depthSwapchain_;
  }

 private:
  XrSwapchain createXrSwapchain(XrSwapchainUsageFlags extraUsageFlags, int64_t format);

 private:
  std::unique_ptr<impl::XrSwapchainProviderImpl> impl_;
  std::shared_ptr<igl::shell::Platform> platform_;
  const XrSession& session_;
  const XrViewConfigurationView& viewport_;

  int64_t selectedColorFormat_;
  int64_t selectedDepthFormat_;
  XrSwapchain colorSwapchain_ = XR_NULL_HANDLE;
  XrSwapchain depthSwapchain_ = XR_NULL_HANDLE;
  uint32_t currentImageIndex_;
  const uint32_t numViews_ =
      1; // The number of layers of the underlying swapchain image would match numViews_.
};
} // namespace igl::shell::openxr
