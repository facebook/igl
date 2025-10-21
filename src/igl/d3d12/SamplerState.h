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

  bool isYUV() const override { return false; }

 private:
  D3D12_SAMPLER_DESC desc_;
};

} // namespace igl::d3d12
