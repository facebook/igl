/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <vector>

#include <shell/openxr/XrPlatform.h>

#include <igl/Device.h>
#include <igl/Texture.h>

namespace igl::shell::openxr::impl {
class XrSwapchainProviderImpl {
 public:
  virtual ~XrSwapchainProviderImpl() = default;
  virtual int64_t preferredColorFormat() const = 0;
  virtual int64_t preferredDepthFormat() const = 0;
  virtual void enumerateImages(igl::IDevice& device,
                               XrSwapchain colorSwapchain,
                               XrSwapchain depthSwapchain,
                               int64_t selectedColorFormat,
                               int64_t selectedDepthFormat,
                               const XrViewConfigurationView& viewport,
                               uint32_t numViews) = 0;
  virtual igl::SurfaceTextures getSurfaceTextures(igl::IDevice& device,
                                                  const XrSwapchain& colorSwapchain,
                                                  const XrSwapchain& depthSwapchain,
                                                  int64_t selectedColorFormat,
                                                  int64_t selectedDepthFormat,
                                                  const XrViewConfigurationView& viewport,
                                                  uint32_t numViews) = 0;

 protected:
  std::vector<std::shared_ptr<igl::ITexture>> colorTextures_;
  std::vector<std::shared_ptr<igl::ITexture>> depthTextures_;
};
} // namespace igl::shell::openxr::impl
