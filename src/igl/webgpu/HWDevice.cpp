/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/HWDevice.h>

#include <utility>

namespace igl::webgpu {

namespace {

HWDeviceType toHWDeviceType(WGPUAdapterType type) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (type) {
  case WGPUAdapterType_DiscreteGPU:
    return HWDeviceType::DiscreteGpu;
  case WGPUAdapterType_IntegratedGPU:
    return HWDeviceType::IntegratedGpu;
  case WGPUAdapterType_CPU:
    return HWDeviceType::SoftwareGpu;
  default:
    return HWDeviceType::Unknown;
  }
}

} // namespace

std::unique_ptr<WebGPUContext> HWDevice::createContext(const WebGPUContextDesc& desc,
                                                       Result* IGL_NULLABLE outResult) {
  return WebGPUContext::create(desc, outResult);
}

std::vector<HWDeviceDesc> HWDevice::queryDevices(const WebGPUContext& ctx,
                                                 const HWDeviceQueryDesc& desc,
                                                 Result* IGL_NULLABLE outResult) {
  std::vector<HWDeviceDesc> devices;
  if (ctx.getAdapter() == nullptr) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "No WebGPU adapter");
    return devices;
  }
  const HWDeviceType type = toHWDeviceType(ctx.getAdapterType());
  if (desc.hardwareType == HWDeviceType::Unknown || desc.hardwareType == type) {
    devices.emplace_back(reinterpret_cast<uintptr_t>(ctx.getAdapter()),
                         type,
                         ctx.getVendorId(),
                         ctx.getAdapterName(),
                         ctx.getAdapterVendor());
  }
  Result::setOk(outResult);
  return devices;
}

std::unique_ptr<Device> HWDevice::create(std::unique_ptr<WebGPUContext> ctx,
                                         const HWDeviceDesc& desc,
                                         Result* IGL_NULLABLE outResult) {
  if (!ctx) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "WebGPUContext is null");
    return nullptr;
  }
  if (desc.guid != reinterpret_cast<uintptr_t>(ctx->getAdapter())) {
    Result::setResult(
        outResult, Result::Code::ArgumentInvalid, "HWDeviceDesc does not describe this adapter");
    return nullptr;
  }
  Result result = ctx->initDevice();
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_unique<Device>(std::move(ctx));
}

std::unique_ptr<Device> HWDevice::createWithWGPUDevice(WGPUInstance IGL_NULLABLE instance,
                                                       WGPUDevice IGL_NULLABLE device,
                                                       const WebGPUContextDesc& desc,
                                                       Result* IGL_NULLABLE outResult) {
  auto ctx = WebGPUContext::createWithDevice(desc, instance, device, outResult);
  return ctx ? std::make_unique<Device>(std::move(ctx)) : nullptr;
}

#if IGL_PLATFORM_EMSCRIPTEN
std::unique_ptr<Device> HWDevice::createWithJsDevice(const WebGPUContextDesc& desc,
                                                     Result* IGL_NULLABLE outResult) {
  auto ctx = WebGPUContext::createWithJsDevice(desc, outResult);
  return ctx ? std::make_unique<Device>(std::move(ctx)) : nullptr;
}
#endif

} // namespace igl::webgpu
