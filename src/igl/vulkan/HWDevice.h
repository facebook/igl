/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>

#include <igl/Device.h>

namespace igl {
namespace vulkan {

class VulkanContext;
struct VulkanContextConfig;

class HWDevice final {
 public:
  static std::unique_ptr<VulkanContext> createContext(
      const VulkanContextConfig& config,
      void* window,
      size_t numExtraInstanceExtensions = 0,
      const char** extraInstanceExtensions = nullptr,
      void* display = nullptr);

  static std::vector<HWDeviceDesc> queryDevices(VulkanContext& ctx,
                                                HWDeviceType deviceType,
                                                Result* outResult = nullptr);

  /*
   * @brief Create a new vulkan::Device
   *        Only 1 device can be created for Vulkan. The new device will take ownership of
   * VulkanContext. If the process failes, the provided VulkanContext is destroyed.
   */

  static std::unique_ptr<IDevice> create(std::unique_ptr<VulkanContext> ctx,
                                         const HWDeviceDesc& desc,
                                         uint32_t width,
                                         uint32_t height,
                                         size_t numExtraDeviceExtensions = 0,
                                         const char** extraDeviceExtensions = nullptr,
                                         Result* outResult = nullptr);
};

} // namespace vulkan
} // namespace igl
