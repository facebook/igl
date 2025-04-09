/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/Device.h>
#include <igl/opengl/glx/PlatformDevice.h>

namespace igl::opengl::glx {

class Device final : public igl::opengl::Device {
 public:
  explicit Device(std::unique_ptr<IContext> context);
  ~Device() override;

  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override;

 protected:
  PlatformDevice platformDevice_;
};

} // namespace igl::opengl::glx
