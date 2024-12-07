/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/DeviceFeatures.h>
#include <string>

namespace igl::shell {

struct RenderSessionConfig {
  std::string displayName;
  BackendVersion backendVersion;
  igl::TextureFormat swapchainColorTextureFormat = igl::TextureFormat::BGRA_UNorm8;
  igl::ColorSpace swapchainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;
};

} // namespace igl::shell
