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

namespace igl::shell::openxr::mobile {
class XrSwapchainProviderImplGLES final : public impl::XrSwapchainProviderImpl {
 public:
  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] std::vector<int64_t> preferredColorFormats() const noexcept final {
    return {GL_SRGB8_ALPHA8};
  }
  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] std::vector<int64_t> preferredDepthFormats() const noexcept final {
    return {GL_DEPTH_COMPONENT16};
  }

  void enumerateImages(igl::IDevice& device,
                       XrSwapchain colorSwapchain,
                       XrSwapchain depthSwapchain,
                       const impl::SwapchainImageInfo& swapchainImageInfo,
                       uint8_t numViews) noexcept final;

  [[nodiscard]] igl::SurfaceTextures getSurfaceTextures(
      igl::IDevice& device,
      XrSwapchain colorSwapchain,
      XrSwapchain depthSwapchain,
      const impl::SwapchainImageInfo& swapchainImageInfo,
      uint8_t numViews) noexcept final;

 private:
  std::vector<uint32_t> colorImages_;
  std::vector<uint32_t> depthImages_;
};
} // namespace igl::shell::openxr::mobile
