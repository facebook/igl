/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/Device.h>
#include <igl/opengl/macos/PlatformDevice.h>

namespace igl::opengl::macos {

class Device final : public ::igl::opengl::Device {
 public:
  explicit Device(std::unique_ptr<IContext> context);
  ~Device() override = default;

  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override;

 private:
  PlatformDevice platformDevice_;
};

} // namespace igl::opengl::macos
