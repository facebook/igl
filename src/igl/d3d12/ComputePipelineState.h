/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/ComputePipelineState.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class ComputePipelineState final : public IComputePipelineState {
 public:
  ComputePipelineState() = default;
  ~ComputePipelineState() override = default;

  std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override;

 private:
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
};

} // namespace igl::d3d12
