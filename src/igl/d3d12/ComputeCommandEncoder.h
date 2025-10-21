/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/ComputeCommandEncoder.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class ComputeCommandEncoder final : public IComputeCommandEncoder {
 public:
  ~ComputeCommandEncoder() override = default;

  void endEncoding() override;
  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;
  void insertDebugEventLabel(const std::string& label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;

  void bindComputePipelineState(const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                           const Dimensions& threadgroupSize,
                           const Dependencies& dependencies = {}) override;
  void bindBuffer(uint32_t index, IBuffer* buffer, size_t bufferOffset = 0) override;
  void bindBytes(size_t index, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset = 0) override;
  void bindTexture(uint32_t index, ITexture* texture) override;
  void bindBindGroup(BindGroupTextureHandle handle) override;
  void bindBindGroup(BindGroupBufferHandle handle, uint32_t dynamicOffset) override;

 private:
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
};

} // namespace igl::d3d12
