/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <webgpu/webgpu.h>
#include <igl/PlatformDevice.h>

namespace igl::webgpu {

class Device;
class WebGPUContext;

/// @brief WebGPU-specific device functionality. Reach it through
/// IDevice::getPlatformDevice<webgpu::PlatformDevice>().
class PlatformDevice final : public IPlatformDevice {
 public:
  static constexpr PlatformDeviceType kType = PlatformDeviceType::WebGPU;

  explicit PlatformDevice(Device& device) : device_(device) {}

  [[nodiscard]] WebGPUContext& getContext() const noexcept;
  /// The returned handles are not retained; they live as long as the device.
  [[nodiscard]] WGPUInstance IGL_NULLABLE getWGPUInstance() const noexcept;
  [[nodiscard]] WGPUAdapter IGL_NULLABLE getWGPUAdapter() const noexcept;
  [[nodiscard]] WGPUDevice IGL_NULLABLE getWGPUDevice() const noexcept;
  [[nodiscard]] WGPUQueue IGL_NULLABLE getWGPUQueue() const noexcept;
  /// Whether the device was created with `feature`.
  [[nodiscard]] bool hasWGPUFeature(WGPUFeatureName feature) const;

  /// See WebGPUContext::setSuspensionAllowed(). Browser exports that are not JSPI exports call
  /// setSuspensionAllowed(false) so that waits fail instead of trapping.
  void setSuspensionAllowed(bool allowed) noexcept;
  [[nodiscard]] bool isSuspensionAllowed() const noexcept;
  /// Returns and clears the WebGPU errors latched by create calls that did not wait for them.
  [[nodiscard]] std::vector<Result> takeErrors();

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override {
    return t == kType;
  }

 private:
  Device& device_;
};

} // namespace igl::webgpu
