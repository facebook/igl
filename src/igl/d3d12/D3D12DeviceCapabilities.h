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

  // D3D12_RENDER_PASS_TIER_0 when the query fails.
  [[nodiscard]] D3D12_RENDER_PASS_TIER getRenderPassesTier() const {
    return deviceOptions5_.RenderPassesTier;
  }

  // Whether the adapter of node 0 is a tile-based renderer. False when the query fails.
  [[nodiscard]] bool isTileBasedRenderer() const {
    return architecture_.TileBasedRenderer != FALSE;
  }

  // The OPTIONS18 query fails on runtimes that predate it, which leaves the answer unknown.
  [[nodiscard]] bool isRenderPassesValidKnown() const {
    return renderPassesValidKnown_;
  }

  // False when the answer is unknown.
  [[nodiscard]] bool areRenderPassesValid() const {
    return renderPassesValid_;
  }

 private:
  void validateDeviceLimits(D3D12Context& ctx);

  D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 deviceOptions1_ = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 deviceOptions5_ = {};
  D3D12_FEATURE_DATA_ARCHITECTURE architecture_ = {};
  bool renderPassesValidKnown_ = false;
  bool renderPassesValid_ = false;
};

} // namespace igl::d3d12
