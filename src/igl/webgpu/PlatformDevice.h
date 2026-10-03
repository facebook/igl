/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/PlatformDevice.h>

namespace igl::webgpu {

class Device;

class PlatformDevice final : public IPlatformDevice {
 public:
  static constexpr PlatformDeviceType kType = PlatformDeviceType::WebGPU;

  explicit PlatformDevice(Device& device) : device_(device) {}

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override {
    return t == kType;
  }

 private:
  [[maybe_unused]] Device& device_;
};

} // namespace igl::webgpu
