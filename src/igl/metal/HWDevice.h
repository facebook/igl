/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Device.h>
#include <igl/HWDevice.h>

#ifdef __OBJC__
#include <igl/metal/Device.h>

@protocol MTLDevice;
#endif

namespace igl::metal {

class HWDevice {
 public:
  /// Enumerate all available GPUs that match the provided query
  /// @param desc source image.
  /// @param outResult optional result
  /// @return descriptors array of device descriptors that match the query. Array is empty if
  /// there's no match.
  std::vector<HWDeviceDesc> queryDevices(const HWDeviceQueryDesc& desc, Result* outResult);
  std::unique_ptr<IDevice> create(const HWDeviceDesc& desc, Result* outResult);

  /// Shorthand to create a given device via MTLCreateSystemDefaultDevice().
  std::unique_ptr<IDevice> createWithSystemDefaultDevice(Result* outResult);

#ifdef __OBJC__
  /// Create a device with a given existing MTLDevice.
  std::unique_ptr<Device> createWithMTLDevice(id<MTLDevice> device, Result* outResult);
#endif
};

} // namespace igl::metal
