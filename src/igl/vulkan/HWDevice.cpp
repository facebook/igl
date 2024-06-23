/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "HWDevice.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>

namespace igl::vulkan {

std::unique_ptr<VulkanContext> HWDevice::createContext(const VulkanContextConfig& config,
                                                       void* window,
                                                       size_t numExtraInstanceExtensions,
                                                       const char** extraInstanceExtensions,
                                                       void* display) {
  return std::make_unique<VulkanContext>(
      config, window, numExtraInstanceExtensions, extraInstanceExtensions, display);
}

std::vector<HWDeviceDesc> HWDevice::queryDevices(VulkanContext& ctx,
                                                 const HWDeviceQueryDesc& desc,
                                                 Result* outResult) {
  std::vector<HWDeviceDesc> outDevices;

  Result::setResult(outResult, ctx.queryDevices(desc, outDevices));

  return outDevices;
}

std::unique_ptr<IDevice> HWDevice::create(std::unique_ptr<VulkanContext> ctx,
                                          const HWDeviceDesc& desc,
                                          uint32_t width,
                                          uint32_t height,
                                          size_t numExtraDeviceExtensions,
                                          const char** extraDeviceExtensions,
                                          Result* outResult) {
  IGL_ASSERT(ctx);

  auto result = ctx->initContext(desc, numExtraDeviceExtensions, extraDeviceExtensions);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return nullptr;
  }

  if (width > 0 && height > 0) {
    result = ctx->initSwapchain(width, height);

    Result::setResult(outResult, result);
  }

  return result.isOk() ? std::make_unique<igl::vulkan::Device>(std::move(ctx)) : nullptr;
}

} // namespace igl::vulkan
