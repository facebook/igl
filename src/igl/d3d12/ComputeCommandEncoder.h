/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/ComputeCommandEncoder.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12ResourcesBinder.h>

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

  /**
   * @brief Bind a buffer to a compute shader slot
   *
   * IMPORTANT: For constant buffers (uniform buffers) in compute shaders, bindings MUST be DENSE
   * starting from index 0 with NO GAPS. For example:
   *   - VALID:   bindBuffer(0, ...), bindBuffer(1, ...), bindBuffer(2, ...)
   *   - INVALID: bindBuffer(0, ...), bindBuffer(2, ...) // gap at index 1
   *   - INVALID: bindBuffer(1, ...), bindBuffer(2, ...) // index 0 not bound
   *
   * This constraint is enforced by D3D12ResourcesBinder and will return InvalidOperation if violated.
   * See D3D12ResourcesBinder::updateBufferBindings for implementation details.
   *
   * @param index Buffer slot index (maps to HLSL register b0, b1, etc. for CBVs)
   * @param buffer Buffer to bind
   * @param offset Offset in bytes into the buffer
   * @param bufferSize Size of the buffer region to bind
   */
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

  // Centralized resource binding management.
  D3D12ResourcesBinder resourcesBinder_;

  // Cached GPU handles for resources
  // IMPORTANT: Bindings must be DENSE and start at slot 0 for each table.
  // SetComputeRootDescriptorTable always uses cached*Handles_[0] as the base,
  // so binding only higher slots (e.g., slot 1 without slot 0) will fail.
  static constexpr size_t kMaxComputeBuffers = 8;
  // Increased from 8 to 16 to match IGL_TEXTURE_SAMPLERS_MAX contract.
  static constexpr size_t kMaxComputeTextures = IGL_TEXTURE_SAMPLERS_MAX;  // 16
  // Increased from 4 to 16 to match IGL_TEXTURE_SAMPLERS_MAX contract.
  static constexpr size_t kMaxComputeSamplers = IGL_TEXTURE_SAMPLERS_MAX;  // 16

  D3D12_GPU_DESCRIPTOR_HANDLE cachedUavHandles_[kMaxComputeBuffers] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSrvHandles_[kMaxComputeTextures] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSamplerHandles_[kMaxComputeSamplers] = {};
  D3D12_GPU_VIRTUAL_ADDRESS cachedCbvAddresses_[kMaxComputeBuffers] = {};
  // Track CBV sizes for descriptor creation.
  size_t cachedCbvSizes_[kMaxComputeBuffers] = {};

  size_t boundUavCount_ = 0;
  size_t boundSrvCount_ = 0;
  size_t boundCbvCount_ = 0;
  size_t boundSamplerCount_ = 0;

  // Cache CBV descriptor indices to avoid per-dispatch allocation.
  uint32_t cachedCbvBaseIndex_ = 0;
  uint32_t cachedCbvPageIndex_ = UINT32_MAX;  // Track heap page for invalidation
  bool cbvBindingsDirty_ = true;  // Track if CBV bindings have changed

  // Track UAV resources for precise synchronization barriers.
  // Tracks UAV resources bound via bindBuffer (storage buffers) and bindImageTexture (RW textures).
  ID3D12Resource* boundUavResources_[kMaxComputeBuffers] = {};
};

} // namespace igl::d3d12
