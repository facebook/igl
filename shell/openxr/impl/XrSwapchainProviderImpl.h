/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>

#include <igl/Device.h>
#include <igl/Texture.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace igl::shell::openxr::impl {
constexpr int64_t kSwapchainImageInvalidFormat = -1;

struct SwapchainImageInfo {
  int64_t colorFormat = kSwapchainImageInvalidFormat;
  int64_t depthFormat = kSwapchainImageInvalidFormat;
  uint32_t imageWidth = 0;
  uint32_t imageHeight = 0;
};

inline bool operator==(const SwapchainImageInfo& lhs, const SwapchainImageInfo& rhs) noexcept {
  return (lhs.colorFormat == rhs.colorFormat && lhs.depthFormat == rhs.depthFormat &&
          lhs.imageWidth == rhs.imageWidth && lhs.imageHeight == rhs.imageHeight);
}

class XrSwapchainProviderImpl {
 public:
  virtual ~XrSwapchainProviderImpl() noexcept = default;

  [[nodiscard]] virtual std::vector<int64_t> preferredColorFormats() const noexcept = 0;
  [[nodiscard]] virtual std::vector<int64_t> preferredDepthFormats() const noexcept = 0;

  virtual void enumerateImages(IDevice& device,
                               XrSwapchain colorSwapchain,
                               XrSwapchain depthSwapchain,
                               const SwapchainImageInfo& swapchainImageInfo,
                               uint8_t numViews) noexcept = 0;

  [[nodiscard]] virtual SurfaceTextures getSurfaceTextures(
      IDevice& device,
      XrSwapchain colorSwapchain,
      XrSwapchain depthSwapchain,
      const SwapchainImageInfo& swapchainImageInfo,
      uint8_t numViews) noexcept = 0;

 protected:
  std::vector<std::shared_ptr<ITexture>> colorTextures_;
  std::vector<std::shared_ptr<ITexture>> depthTextures_;
};
} // namespace igl::shell::openxr::impl
