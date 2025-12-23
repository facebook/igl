/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class D3D12Context;

class D3D12DeviceCapabilities {
 public:
  void initialize(D3D12Context& ctx);

  [[nodiscard]] const D3D12_FEATURE_DATA_D3D12_OPTIONS& getOptions() const {
    return deviceOptions_;
  }

  [[nodiscard]] const D3D12_FEATURE_DATA_D3D12_OPTIONS1& getOptions1() const {
    return deviceOptions1_;
  }

  [[nodiscard]] D3D12_RESOURCE_BINDING_TIER getResourceBindingTier() const {
    return deviceOptions_.ResourceBindingTier;
  }

 private:
  void validateDeviceLimits(D3D12Context& ctx);

  D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 deviceOptions1_ = {};
};

} // namespace igl::d3d12
