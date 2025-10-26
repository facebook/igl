/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ComputePipelineState.h>

namespace igl::d3d12 {

ComputePipelineState::ComputePipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
                                           Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
                                           const std::string& debugName)
    : pipelineState_(std::move(pipelineState)), rootSignature_(std::move(rootSignature)) {
  // Set D3D12 object names for PIX debugging
  if (pipelineState_.Get() && !debugName.empty()) {
    std::wstring wideName(debugName.begin(), debugName.end());
    pipelineState_->SetName((L"ComputePSO_" + wideName).c_str());
    IGL_LOG_INFO("ComputePipelineState: Set PIX debug name 'ComputePSO_%s'\n", debugName.c_str());
  }
  if (rootSignature_.Get() && !debugName.empty()) {
    std::wstring wideName(debugName.begin(), debugName.end());
    rootSignature_->SetName((L"ComputeRootSig_" + wideName).c_str());
    IGL_LOG_INFO("ComputePipelineState: Set PIX root signature name 'ComputeRootSig_%s'\n", debugName.c_str());
  }
}

std::shared_ptr<IComputePipelineState::IComputePipelineReflection>
ComputePipelineState::computePipelineReflection() {
  return nullptr;
}

} // namespace igl::d3d12
