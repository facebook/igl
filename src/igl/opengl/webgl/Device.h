/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#ifndef __EMSCRIPTEN__
#error "Platform not supported"
#endif

#include <igl/opengl/Device.h>
#include <igl/opengl/webgl/PlatformDevice.h>

namespace igl::opengl::webgl {

class Device final : public ::igl::opengl::Device {
 public:
  explicit Device(std::unique_ptr<IContext> context);
  ~Device() override;

  const PlatformDevice& getPlatformDevice() const noexcept override;

 private:
  PlatformDevice platformDevice_;
};

} // namespace igl::opengl::webgl
