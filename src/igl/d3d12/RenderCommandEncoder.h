/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12ResourcesBinder.h>
#include <igl/d3d12/RenderPipelineState.h>
#include <cstdint>
#include <vector>

namespace igl::d3d12 {

class CommandBuffer;

/**
 * @brief D3D12 implementation of render command encoder
 *
 * IMPORTANT BINDING PRECEDENCE NOTES:
 * ====================================
 * This encoder supports multiple ways to bind shader resources (textures, buffers, samplers).
 * Some binding methods share the same D3D12 root parameters, which means the LAST binding wins:
 *
 * 1. SRV Table (Root Parameter 4):
 *    - Textures bound via bindTexture() or D3D12ResourcesBinder
 *    - Storage buffers (read-only) bound via bindBindGroup(BindGroupBufferHandle)
 *    - If you bind BOTH textures and storage buffer SRVs, the last binding before draw() wins
 *    - Application code must coordinate which binding method to use per draw call
 *
 * 2. Sampler Table (Root Parameter 5):
 *    - Samplers bound via bindSamplerState() or D3D12ResourcesBinder
 *
 * 3. CBV Table (Root Parameter 3):
 *    - Constant buffers b3-b15 bound via bindBindGroup(BindGroupBufferHandle)
 *
 * See individual binding method documentation for details.
 */
class RenderCommandEncoder final : public IRenderCommandEncoder {
 public:
  RenderCommandEncoder(CommandBuffer& commandBuffer,
                       const std::shared_ptr<IFramebuffer>& framebuffer);
  ~RenderCommandEncoder() override = default;

  // Initialize encoder and setup render targets
  // IMPORTANT: Must be called exactly once after construction by CommandBuffer::createRenderCommandEncoder.
  // Calling multiple times will result in resource leaks and undefined behavior.
  // Debug builds will assert if called more than once.
  void begin(const RenderPassDesc& renderPass);

  void endEncoding() override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;
  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  void bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset = 0) override;
  void bindIndexBuffer(IBuffer& buffer, IndexFormat format, size_t bufferOffset = 0) override;

  void bindBytes(size_t index, uint8_t target, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset = 0) override;
  void bindSamplerState(size_t index, uint8_t target, ISamplerState* samplerState) override;
  void bindTexture(size_t index, uint8_t target, ITexture* texture) override;
  void bindTexture(size_t index, ITexture* texture) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;

  void draw(size_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t baseInstance = 0) override;
  void drawIndexed(size_t indexCount,
                   uint32_t instanceCount = 1,
                   uint32_t firstIndex = 0,
                   int32_t vertexOffset = 0,
                   uint32_t baseInstance = 0) override;
  void multiDrawIndirect(IBuffer& indirectBuffer,
                        size_t indirectBufferOffset,
                        uint32_t drawCount,
                        uint32_t stride = 0) override;
  void multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                               size_t indirectBufferOffset,
                               uint32_t drawCount,
                               uint32_t stride = 0) override;

  void setStencilReferenceValue(uint32_t value) override;
  void setBlendColor(const Color& color) override;
  void setDepthBias(float depthBias, float slopeScale, float clamp) override;

  // ICommandEncoder interface
  void pushDebugGroupLabel(const char* label, const Color& color) const override;
  void insertDebugEventLabel(const char* label, const Color& color) const override;
  void popDebugGroupLabel() const override;

  // Additional IRenderCommandEncoder interface
  void bindBuffer(uint32_t index, IBuffer* buffer, size_t offset, size_t bufferSize) override;
  void bindBindGroup(BindGroupTextureHandle handle) override;
  void bindBindGroup(BindGroupBufferHandle handle,
                     uint32_t numDynamicOffsets,
                     const uint32_t* dynamicOffsets) override;

 private:
 CommandBuffer& commandBuffer_;
  ID3D12GraphicsCommandList* commandList_;

  // Centralized resource binding management.
  D3D12ResourcesBinder resourcesBinder_;

  // Guard against multiple begin() calls.
  // begin() allocates RTV/DSV descriptors and sets up state that should only happen once
  bool hasBegun_ = false;

  // Cache current vertex stride from bound pipeline's input layout
  UINT currentVertexStride_ = 0;
  // Optional per-slot strides fetched from pipeline
  UINT vertexStrides_[IGL_BUFFER_BINDINGS_MAX] = {};

  // Offscreen RTV/DSV support
  std::shared_ptr<IFramebuffer> framebuffer_;
  // If DescriptorHeapManager is available, we borrow indices from its heaps.
  // Otherwise, we fall back to small ad-hoc heaps (constructor local scope).
  std::vector<uint32_t> rtvIndices_;
  uint32_t dsvIndex_ = UINT32_MAX;
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

  // Per-frame descriptor heaps (set in constructor from D3D12Context)
  // CRITICAL: These MUST be per-frame isolated heaps, NOT shared DescriptorHeapManager heaps
  ID3D12DescriptorHeap* cbvSrvUavHeap_ = nullptr;
  ID3D12DescriptorHeap* samplerHeap_ = nullptr;

  // Cached descriptor table GPU handles
  // These are set by bindTexture/bindSamplerState and used in drawIndexed
  // to avoid invalidation by multiple SetDescriptorHeaps calls
  // IMPORTANT: Bindings must be DENSE and start at slot 0 for each table.
  // SetGraphicsRootDescriptorTable always uses cachedTextureGpuHandles_[0] as the base,
  // so binding only higher slots (e.g., slot 1 without slot 0) will fail.
  D3D12_GPU_DESCRIPTOR_HANDLE cachedTextureGpuHandle_{};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSamplerGpuHandle_{};
  // Support up to IGL_TEXTURE_SAMPLERS_MAX textures/samplers (t0-t15, s0-s15)
  D3D12_GPU_DESCRIPTOR_HANDLE cachedTextureGpuHandles_[IGL_TEXTURE_SAMPLERS_MAX] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSamplerGpuHandles_[IGL_TEXTURE_SAMPLERS_MAX] = {};
  size_t cachedTextureCount_ = 0;
  size_t cachedSamplerCount_ = 0;

  // Track whether bindBindGroup was explicitly called (vs storage buffer SRV or binder paths)
  // This decouples bindBindGroup usage from cachedTextureCount_/cachedSamplerCount_
  bool usedBindGroup_ = false;

  // Cached vertex buffer bindings
  // Store binding info and apply in draw calls after pipeline state is bound
  struct CachedVertexBuffer {
    D3D12_GPU_VIRTUAL_ADDRESS bufferLocation = 0;
    UINT sizeInBytes = 0;
    bool bound = false;
  };
  CachedVertexBuffer cachedVertexBuffers_[IGL_BUFFER_BINDINGS_MAX] = {};

  // Cached index buffer binding
  struct CachedIndexBuffer {
    D3D12_GPU_VIRTUAL_ADDRESS bufferLocation = 0;
    UINT sizeInBytes = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool bound = false;
  };
  CachedIndexBuffer cachedIndexBuffer_ = {};

  // Track which constant buffer root parameters have been bound
  // D3D12 requires all root parameters to be set before drawing
  // Root parameter 1 = b0 (UniformsPerFrame) - root descriptor
  // Root parameter 2 = b1 (UniformsPerObject) - root descriptor
  D3D12_GPU_VIRTUAL_ADDRESS cachedConstantBuffers_[2] = {0, 0}; // b0, b1
  bool constantBufferBound_[2] = {false, false};

  // Cached CBV descriptor table for b2-b15 (root parameter 3)
  // Supports up to 14 additional uniform buffers via descriptor table
  D3D12_GPU_DESCRIPTOR_HANDLE cachedCbvTableGpuHandles_[IGL_BUFFER_BINDINGS_MAX] = {};
  bool cbvTableBound_[IGL_BUFFER_BINDINGS_MAX] = {};
  size_t cbvTableCount_ = 0;

  // G-001: Barrier batching infrastructure
  // Accumulates resource barriers and flushes them before draw/dispatch calls
  // This reduces D3D12 API overhead and allows driver optimization
  std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers_;

  // Flushes all pending barriers to the command list
  void flushBarriers();

  // Queue a barrier for batched submission
  void queueBarrier(const D3D12_RESOURCE_BARRIER& barrier);

  // Dynamic PSO selection (Vulkan-style pattern)
  // Stores actual framebuffer formats captured in begin()
  // Used to select correct PSO variant at draw time
  D3D12RenderPipelineDynamicState dynamicState_;

  // Cached render pipeline state for dynamic PSO variant selection
  const RenderPipelineState* currentRenderPipelineState_ = nullptr;
};

} // namespace igl::d3d12
