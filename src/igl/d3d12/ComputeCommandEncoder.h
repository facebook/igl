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

class CommandBuffer;
class ComputePipelineState;

class ComputeCommandEncoder final : public IComputeCommandEncoder {
 public:
  explicit ComputeCommandEncoder(CommandBuffer& commandBuffer);
  ~ComputeCommandEncoder() override = default;

  void endEncoding() override;

  void bindComputePipelineState(const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                           const Dimensions& threadgroupSize,
                           const Dependencies& dependencies = {}) override;
  void bindPushConstants(const void* data, size_t length, size_t offset = 0) override;
  void bindTexture(uint32_t index, ITexture* texture) override;
  void bindBuffer(uint32_t index, IBuffer* buffer, size_t offset = 0, size_t bufferSize = 0) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;
  void bindBytes(uint32_t index, const void* data, size_t length) override;
  void bindImageTexture(uint32_t index, ITexture* texture, TextureFormat format) override;
  void bindSamplerState(uint32_t index, ISamplerState* samplerState) override;

  // Debug labels
  void pushDebugGroupLabel(const char* label, const Color& color) const override;
  void insertDebugEventLabel(const char* label, const Color& color) const override;
  void popDebugGroupLabel() const override;

 private:
  CommandBuffer& commandBuffer_;
  const ComputePipelineState* currentPipeline_ = nullptr;
  bool isEncoding_ = false;

  // Cached GPU handles for resources
  // IMPORTANT: Bindings must be DENSE and start at slot 0 for each table.
  // SetComputeRootDescriptorTable always uses cached*Handles_[0] as the base,
  // so binding only higher slots (e.g., slot 1 without slot 0) will fail.
  static constexpr size_t kMaxComputeBuffers = 8;
  // P1_DX12-007: Increased from 8 to 16 to match IGL_TEXTURE_SAMPLERS_MAX contract
  static constexpr size_t kMaxComputeTextures = IGL_TEXTURE_SAMPLERS_MAX;  // 16
  // P1_DX12-007: Increased from 4 to 16 to match IGL_TEXTURE_SAMPLERS_MAX contract
  static constexpr size_t kMaxComputeSamplers = IGL_TEXTURE_SAMPLERS_MAX;  // 16

  D3D12_GPU_DESCRIPTOR_HANDLE cachedUavHandles_[kMaxComputeBuffers] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSrvHandles_[kMaxComputeTextures] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSamplerHandles_[kMaxComputeSamplers] = {};
  D3D12_GPU_VIRTUAL_ADDRESS cachedCbvAddresses_[kMaxComputeBuffers] = {};
  // P1_DX12-FIND-04: Track CBV sizes for descriptor creation
  size_t cachedCbvSizes_[kMaxComputeBuffers] = {};

  size_t boundUavCount_ = 0;
  size_t boundSrvCount_ = 0;
  size_t boundCbvCount_ = 0;
  size_t boundSamplerCount_ = 0;
};

} // namespace igl::d3d12
