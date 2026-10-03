/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <webgpu/webgpu.h>
#include <igl/DeviceFeatures.h>

namespace igl::webgpu {

/// @brief Answers ICapabilities queries from the features and limits a WGPUDevice was created with.
class DeviceFeatureSet final {
 public:
  /// `device` must outlive this object.
  explicit DeviceFeatureSet(WGPUDevice IGL_NULLABLE device);

  [[nodiscard]] bool hasFeature(DeviceFeatures feature) const;
  [[nodiscard]] bool hasRequirement(DeviceRequirement requirement) const;
  [[nodiscard]] bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const;
  [[nodiscard]] ICapabilities::TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat format) const;

  [[nodiscard]] bool hasWGPUFeature(WGPUFeatureName feature) const;
  [[nodiscard]] const WGPULimits& getLimits() const noexcept {
    return limits_;
  }

 private:
  WGPUDevice IGL_NULLABLE device_;
  WGPULimits limits_ = WGPU_LIMITS_INIT;
  bool limitsValid_ = false;
};

} // namespace igl::webgpu
