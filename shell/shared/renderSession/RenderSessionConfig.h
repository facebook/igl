/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <igl/DeviceFeatures.h>

namespace igl::shell {

struct RenderSessionConfig {
  std::string displayName;
  BackendVersion backendVersion;
  TextureFormat swapchainColorTextureFormat = igl::TextureFormat::BGRA_UNorm8;
  TextureFormat depthTextureFormat = igl::TextureFormat::Z_UNorm16;
  ColorSpace swapchainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;
};

} // namespace igl::shell
