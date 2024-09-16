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
  igl::TextureFormat colorFramebufferFormat = igl::TextureFormat::BGRA_UNorm8;
  uint32_t width = 1024;
  uint32_t height = 768;
  bool fullscreen = false;
};

} // namespace igl::shell
