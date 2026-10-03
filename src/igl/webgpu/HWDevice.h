/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>
#include <igl/HWDevice.h>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

/// @brief Creates WebGPU contexts and devices.
class HWDevice final {
 public:
  /// Creates the instance and requests the adapter described by `desc`.
  [[nodiscard]] static std::unique_ptr<WebGPUContext> createContext(
      const WebGPUContextDesc& desc,
      Result* IGL_NULLABLE outResult = nullptr);

  /// Returns the context's adapter if it matches `desc.hardwareType` (Unknown matches any). Its
  /// guid is the WGPUAdapter handle.
  [[nodiscard]] static std::vector<HWDeviceDesc> queryDevices(
      const WebGPUContext& ctx,
      const HWDeviceQueryDesc& desc,
      Result* IGL_NULLABLE outResult = nullptr);

  /// Requests the device from the context's adapter; `desc` must come from queryDevices(). The new
  /// device owns `ctx`.
  [[nodiscard]] static std::unique_ptr<Device> create(std::unique_ptr<WebGPUContext> ctx,
                                                      const HWDeviceDesc& desc,
                                                      Result* IGL_NULLABLE outResult = nullptr);

  /// Wraps a device created outside IGL; see WebGPUContext::createWithDevice().
  [[nodiscard]] static std::unique_ptr<Device> createWithWGPUDevice(
      WGPUInstance IGL_NULLABLE instance,
      WGPUDevice IGL_NULLABLE device,
      const WebGPUContextDesc& desc = {},
      Result* IGL_NULLABLE outResult = nullptr);
#if IGL_PLATFORM_EMSCRIPTEN
  /// Imports the page's GPUDevice; see WebGPUContext::createWithJsDevice(). Link with
  /// `--js-library emscripten/library_iglwebgpu.js`.
  [[nodiscard]] static std::unique_ptr<Device> createWithJsDevice(
      const WebGPUContextDesc& desc = {},
      Result* IGL_NULLABLE outResult = nullptr);
#endif
};

} // namespace igl::webgpu
