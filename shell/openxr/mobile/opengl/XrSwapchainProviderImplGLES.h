/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>

#include <shell/openxr/impl/XrSwapchainProviderImpl.h>

#include <vector>

namespace igl::shell::openxr::mobile {
class XrSwapchainProviderImplGLES final : public impl::XrSwapchainProviderImpl {
 public:
  int64_t preferredColorFormat() const final {
    return GL_SRGB8_ALPHA8;
  }
  int64_t preferredDepthFormat() const final {
    return GL_DEPTH_COMPONENT16;
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
  std::vector<uint32_t> colorImages_;
  std::vector<uint32_t> depthImages_;
};
} // namespace igl::shell::openxr::mobile
