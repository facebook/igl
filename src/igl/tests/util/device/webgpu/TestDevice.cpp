/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/webgpu/TestDevice.h>

#include <utility>
#include <vector>
#include <igl/webgpu/HWDevice.h>

namespace igl::tests::util::device::webgpu {

std::unique_ptr<igl::webgpu::Device> createTestDevice(const igl::webgpu::WebGPUContextDesc& desc) {
  Result ret;
  auto ctx = igl::webgpu::HWDevice::createContext(desc, &ret);
  if (!ret.isOk() || !ctx) {
    IGL_LOG_ERROR("[Tests] WebGPU context creation failed: %s\n", ret.message.c_str());
    return nullptr;
  }
  const std::vector<HWDeviceDesc> devices =
      igl::webgpu::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);
  if (!ret.isOk() || devices.empty()) {
    IGL_LOG_ERROR("[Tests] No WebGPU adapter: %s\n", ret.message.c_str());
    return nullptr;
  }
  auto device = igl::webgpu::HWDevice::create(std::move(ctx), devices[0], &ret);
  if (!ret.isOk()) {
    IGL_LOG_ERROR("[Tests] WebGPU device creation failed: %s\n", ret.message.c_str());
    return nullptr;
  }
  return device;
}

} // namespace igl::tests::util::device::webgpu
