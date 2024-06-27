/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/HWDevice.h>

#import <Foundation/Foundation.h>

#import <Metal/Metal.h>
#include <igl/Macros.h>
#include <igl/metal/Device.h>

#if IGL_PLATFORM_MACOS
#include <igl/metal/macos/Device.h>
#else
#include <igl/metal/ios/Device.h>
#endif

#include <cstdint>

namespace igl::metal {

namespace {

#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
bool isRemovable(id<MTLDevice> device) {
  if (@available(macOS 10.13, macCatalyst 13.0, *)) {
    return device.removable;
  }
  // device.removable is not available in macOS APIs prior to 10.13 or on iOS
  return false;
}
#endif

} // namespace

HWDeviceType getDeviceType(id<MTLDevice> device);

HWDeviceType getDeviceType(id<MTLDevice> device) {
  IGL_ASSERT(device != nullptr);

#if IGL_PLATFORM_IOS
  return HWDeviceType::DiscreteGpu;
#else
  if (device.lowPower) {
    return HWDeviceType::IntegratedGpu;
  } else if (isRemovable(device)) {
    return HWDeviceType::ExternalGpu;
  } else {
    return HWDeviceType::DiscreteGpu;
  }
#endif
}

std::vector<HWDeviceDesc> HWDevice::queryDevices(IGL_MAYBE_UNUSED const HWDeviceQueryDesc& desc,
                                                 Result* outResult) {
  std::vector<HWDeviceDesc> devices;

#if IGL_PLATFORM_IOS
  id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
  if (metalDevice) {
    // We don't need __bridge_retained here as iOS always provides the same ptr
    HWDeviceDesc deviceDesc((uintptr_t)(__bridge void*)metalDevice,
                            HWDeviceType::DiscreteGpu,
                            0,
                            std::string([metalDevice.name UTF8String]));
    devices.push_back(deviceDesc);
  }
#else
  // Query device for a specific display
  if (desc.displayId) {
    id<MTLDevice> displayDevice =
        CGDirectDisplayCopyCurrentMetalDevice(static_cast<CGDirectDisplayID>(desc.displayId));
    if (displayDevice) {
      const uintptr_t deviceNative = (uintptr_t)(__bridge void*)displayDevice;
      const HWDeviceDesc deviceDesc(deviceNative,
                                    getDeviceType(displayDevice),
                                    0,
                                    std::string([displayDevice.name UTF8String]));
      devices.push_back(deviceDesc);
    }
    Result::setOk(outResult);
    return devices;
  }

  NSArray<id<MTLDevice>>* deviceList = MTLCopyAllDevices();

  // Loop through all devices and return matching ones
  for (id<MTLDevice> device in deviceList) {
    // We don't need __bridge_retained here as iOS always provides the same ptr
    const uintptr_t deviceNative = (uintptr_t)(__bridge void*)device;

    // If we requested an unknown device type, then return
    // all the available devices
    const uint32_t vendorId = 0;
    if (desc.hardwareType == HWDeviceType::Unknown) {
      const HWDeviceDesc deviceDesc(
          deviceNative, desc.hardwareType, vendorId, std::string([device.name UTF8String]));
      devices.push_back(deviceDesc);
    } else if (device.lowPower) {
      if (desc.hardwareType == HWDeviceType::IntegratedGpu) {
        const HWDeviceDesc deviceDesc(deviceNative,
                                      HWDeviceType::IntegratedGpu,
                                      vendorId,
                                      std::string([device.name UTF8String]));
        devices.push_back(deviceDesc);
      }
    } else if (isRemovable(device)) {
      if (desc.hardwareType == HWDeviceType::ExternalGpu) {
        const HWDeviceDesc deviceDesc(deviceNative,
                                      HWDeviceType::ExternalGpu,
                                      vendorId,
                                      std::string([device.name UTF8String]));
        devices.push_back(deviceDesc);
      }
    } else { // Device is NOT Integrated NOR External
      if (desc.hardwareType == HWDeviceType::DiscreteGpu) {
        const HWDeviceDesc deviceDesc(deviceNative,
                                      HWDeviceType::DiscreteGpu,
                                      vendorId,
                                      std::string([device.name UTF8String]));
        devices.push_back(deviceDesc);
      }
    }
  }
#endif

  Result::setOk(outResult);
  return devices;
}

std::unique_ptr<IDevice> HWDevice::create(const HWDeviceDesc& desc, Result* outResult) {
  IGL_ASSERT_MSG(desc.guid != 0L, "Invalid hardwareGuid(%lu)", desc.guid);
  if (desc.guid == 0L) {
    Result::setResult(outResult, Result::Code::Unsupported, "Metal is not supported!");
    return nullptr;
  }

  return createWithMTLDevice((__bridge id<MTLDevice>)(void*)desc.guid, outResult);
}

std::unique_ptr<IDevice> HWDevice::createWithSystemDefaultDevice(Result* outResult) {
  return createWithMTLDevice(MTLCreateSystemDefaultDevice(), outResult);
}

std::unique_ptr<metal::Device> HWDevice::createWithMTLDevice(id<MTLDevice> device,
                                                             Result* outResult) {
  if (!device) {
    Result::setResult(outResult, Result::Code::Unsupported, "Metal is not supported!");
    return nullptr;
  }

#if IGL_PLATFORM_MACOS
  auto iglDevice = std::make_unique<macos::Device>(device);
#else
  auto iglDevice = std::make_unique<ios::Device>(device);
#endif

  Result::setOk(outResult);
  return iglDevice;
}

} // namespace igl::metal
