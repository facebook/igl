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
  XrSwapchainProviderImplGLES(const IDevice& device, TextureFormat preferredColorFormat);

  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] std::vector<int64_t> preferredColorFormats() const noexcept final {
    return {preferredColorFormat_};
  }
  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] std::vector<int64_t> preferredDepthFormats() const noexcept final {
    return {GL_DEPTH_COMPONENT16};
  }

  void enumerateImages(IDevice& device,
                       XrSwapchain colorSwapchain,
                       XrSwapchain depthSwapchain,
                       const impl::SwapchainImageInfo& swapchainImageInfo,
                       uint8_t numViews) noexcept final;

  [[nodiscard]] SurfaceTextures getSurfaceTextures(
      IDevice& device,
      XrSwapchain colorSwapchain,
      XrSwapchain depthSwapchain,
      const impl::SwapchainImageInfo& swapchainImageInfo,
      uint8_t numViews) noexcept final;

 private:
  int64_t preferredColorFormat_;
  std::vector<uint32_t> colorImages_;
  std::vector<uint32_t> depthImages_;
};
} // namespace igl::shell::openxr::mobile
