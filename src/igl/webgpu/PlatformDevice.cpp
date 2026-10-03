/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/PlatformDevice.h>

#include <igl/webgpu/Device.h>

namespace igl::webgpu {

WebGPUContext& PlatformDevice::getContext() const noexcept {
  return device_.getContext();
}

WGPUInstance IGL_NULLABLE PlatformDevice::getWGPUInstance() const noexcept {
  return getContext().getInstance();
}

WGPUAdapter IGL_NULLABLE PlatformDevice::getWGPUAdapter() const noexcept {
  return getContext().getAdapter();
}

WGPUDevice IGL_NULLABLE PlatformDevice::getWGPUDevice() const noexcept {
  return getContext().getDevice();
}

WGPUQueue IGL_NULLABLE PlatformDevice::getWGPUQueue() const noexcept {
  return getContext().getQueue();
}

bool PlatformDevice::hasWGPUFeature(WGPUFeatureName feature) const {
  return device_.getDeviceFeatureSet().hasWGPUFeature(feature);
}

} // namespace igl::webgpu
