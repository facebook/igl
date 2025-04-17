/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <utility>
#include <vector>
#include <igl/Common.h>

namespace igl {
class IDevice;

/**
 * @brief Represents a type of a physical device for graphics/compute purposes
 */
enum class HWDeviceType {
  /// Unknown
  Unknown = 0,
  /// HW GPU - Discrete
  DiscreteGpu = 1,
  /// HW GPU - External
  ExternalGpu = 2,
  /// HW GPU - Integrated
  IntegratedGpu = 3,
  /// SW GPU
  SoftwareGpu = 4,
};

/**
 * @brief Represents a query of a physical device to be requested from the underlying IGL
 * implementation
 */
struct HWDeviceQueryDesc {
  /** @brief Desired hardware type */
  HWDeviceType hardwareType;
  /** @brief If set, ignores hardwareType and returns device assigned to displayId */
  uintptr_t displayId;
  /** @brief Reserved */
  uint32_t flags;

  explicit HWDeviceQueryDesc(HWDeviceType hardwareType,
                             uintptr_t displayId = 0L,
                             uint32_t flags = 0L) :
    hardwareType(hardwareType), displayId(displayId), flags(flags) {}
};

/**
 * @brief  Represents a description of a specific physical device installed in the system
 */
struct HWDeviceDesc {
  /** @brief Implementation-specific identifier of a device */
  uintptr_t guid;
  /** @brief A type of an actual physical device */
  HWDeviceType type;
  /** @brief Implementation-specific name of a device */
  std::string name;
  /** @brief Implementation-specific vendor name */
  std::string vendor;
  /** @brief Unique identifier of a vendor */
  uint32_t vendorId;

  HWDeviceDesc(uintptr_t guid,
               HWDeviceType type,
               uint32_t vendorId = 0,
               std::string name = "",
               std::string vendor = "") :
    guid(guid), type(type), name(std::move(name)), vendor(std::move(vendor)), vendorId(vendorId) {}
};

} // namespace igl
