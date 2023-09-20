/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/PlatformDevice.h>

namespace iglu::sentinel {

/**
 * Sentinel PlatformDevice intended for safe use where access to a real platform device is not
 * available.
 * Use cases include returning a reference to a platform device from a raw pointer when a
 * valid platform device is not available.
 * All methods return nullptr, the default value or an error.
 */
class PlatformDevice final : public igl::IPlatformDevice {
 public:
  explicit PlatformDevice(bool shouldAssert = true);
  [[nodiscard]] bool isType(igl::PlatformDeviceType t) const noexcept final;

 private:
  [[maybe_unused]] bool shouldAssert_;
};

} // namespace iglu::sentinel
