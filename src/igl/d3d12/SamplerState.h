/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/SamplerState.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class SamplerState final : public ISamplerState {
 public:
  explicit SamplerState(const D3D12_SAMPLER_DESC& desc) : desc_(desc) {}
  ~SamplerState() override = default;

  bool isYUV() const noexcept override { return false; }

  const D3D12_SAMPLER_DESC& getDesc() const { return desc_; }

  /// Computes hash value based on D3D12_SAMPLER_DESC fields
  size_t hash() const noexcept;

  /// Compares two SamplerState objects for equality
  bool operator==(const SamplerState& rhs) const noexcept;

 private:
  D3D12_SAMPLER_DESC desc_;
};

} // namespace igl::d3d12
