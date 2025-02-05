/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

/**
 * @brief PlatformDeviceType represents the platform and graphics backend that a class implementing
 * the IPlatformDevice interface supports.
 *
 */
enum class PlatformDeviceType {
  Unknown = 0,
  Metal,
  OpenGL,
  OpenGLEgl,
  OpenGLWgl,
  OpenGLx,
  OpenGLIOS,
  OpenGLMacOS,
  OpenGLWebGL,
  Vulkan,
  // @fb-only
};

/**
 * @brief IPlatformDevice is an interface for platform-specific functionality. It enables
 * type-safety and avoids unsafe downcasts by exposing a method to check the PlatformDeviceType of
 * the IPlatformDevice. Each implementation of IPlatformDevice can contain different
 * platform-specific functionality. For example, a common use-case is to expose a method to create a
 * new igl::ITexture from some platform-specific objects or data, such as a drawable surface on the
 * given platform.
 */
class IPlatformDevice {
  friend class IDevice;

 protected:
  IPlatformDevice() = default;
  /**
   * @brief Check the type of an IPlatformDevice.
   * @returns true if the IPlatformDevice is a given PlatformDeviceType t, otherwise false.
   */
  [[nodiscard]] virtual bool isType(PlatformDeviceType t) const noexcept = 0;

 public:
  virtual ~IPlatformDevice() = default;
};

} // namespace igl
