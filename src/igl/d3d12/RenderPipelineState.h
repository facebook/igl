/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderPipelineState.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class RenderPipelineState final : public IRenderPipelineState {
 public:
  RenderPipelineState(const RenderPipelineDesc& desc,
                      Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
                      Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature);
  ~RenderPipelineState() override = default;

  std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() override;
  void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) override;
  int getIndexByName(const igl::NameHandle& name, ShaderStage stage) const override;
  int getIndexByName(const std::string& name, ShaderStage stage) const override;

  // D3D12-specific accessors
  ID3D12PipelineState* getPipelineState() const { return pipelineState_.Get(); }
  ID3D12RootSignature* getRootSignature() const { return rootSignature_.Get(); }

 private:
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
  std::shared_ptr<IRenderPipelineReflection> reflection_;
};

} // namespace igl::d3d12
