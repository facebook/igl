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
  ComputePipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
                       Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature)
      : pipelineState_(std::move(pipelineState)), rootSignature_(std::move(rootSignature)) {}
  ~ComputePipelineState() override = default;

  std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override;

  // D3D12-specific accessors
  ID3D12PipelineState* getPipelineState() const { return pipelineState_.Get(); }
  ID3D12RootSignature* getRootSignature() const { return rootSignature_.Get(); }

 private:
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
};

} // namespace igl::d3d12
