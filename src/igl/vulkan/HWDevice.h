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
#include <igl/HWDevice.h>

namespace igl::vulkan {

class VulkanContext;
struct VulkanContextConfig;

/// @brief This class provides convenience functions to query, and create devices, as well as to
/// create a VulkanContext object.
class HWDevice final {
 public:
  /** @brief Creates a VulkanContext object with the specified configuration and the extensions
   * provided in the array pointed by `extraInstanceExtensions`.
   * @param config is the configuration used to create the VulkanContext object
   * @param window is a pointer to a native window handle. For windows, it should be a pointer to
   * the Win32 HINSTANCE for the window to associate the surface with. For Android, it should be a
   * pointer to the ANativeWindow. For Xlib, it should be an Xlib Window.
   * @param numExtraInstanceExtensions is the number of additional instance extensions to enable.
   * @param extraInstanceExtensions is a pointer to an array of strings containing the names of the
   * extensions to enable for the context.
   * @param display is a pointer to an Xlib Display connection to the X server. Used only when
   * `VK_USE_PLATFORM_XLIB_KHR` is defined.
   */
  static std::unique_ptr<VulkanContext> createContext(
      const VulkanContextConfig& config,
      void* window,
      size_t numExtraInstanceExtensions = 0,
      const char** extraInstanceExtensions = nullptr,
      void* display = nullptr);

  static std::vector<HWDeviceDesc> queryDevices(VulkanContext& ctx,
                                                const HWDeviceQueryDesc& desc,
                                                Result* outResult = nullptr);

  /*
   * @brief Create a new vulkan::Device
   *        Only 1 device can be created for Vulkan. The new device will take ownership of
   * VulkanContext. If the process fails, the provided VulkanContext is destroyed. If the width and
   * height are greater than 0, this functions also initializes the swapchain.
   */

  static std::unique_ptr<IDevice> create(std::unique_ptr<VulkanContext> ctx,
                                         const HWDeviceDesc& desc,
                                         uint32_t width,
                                         uint32_t height,
                                         size_t numExtraDeviceExtensions = 0,
                                         const char** extraDeviceExtensions = nullptr,
                                         Result* outResult = nullptr);
};

} // namespace igl::vulkan
