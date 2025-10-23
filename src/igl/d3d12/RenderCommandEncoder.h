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
#include <cstdint>

namespace igl::d3d12 {

class CommandBuffer;

class RenderCommandEncoder final : public IRenderCommandEncoder {
 public:
  RenderCommandEncoder(CommandBuffer& commandBuffer,
                       const RenderPassDesc& renderPass,
                       const std::shared_ptr<IFramebuffer>& framebuffer);
  ~RenderCommandEncoder() override = default;

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

 // Simple descriptor allocation (per-frame, reset on next frame)
  UINT nextCbvSrvUavDescriptor_ = 0;
  UINT nextSamplerDescriptor_ = 0;

  // Cache current vertex stride from bound pipeline's input layout
  UINT currentVertexStride_ = 0;
  // Optional per-slot strides fetched from pipeline
  UINT vertexStrides_[IGL_BUFFER_BINDINGS_MAX] = {};

  // Offscreen RTV/DSV support
  std::shared_ptr<IFramebuffer> framebuffer_;
  // If DescriptorHeapManager is available, we borrow indices from its heaps.
  // Otherwise, we fall back to small ad-hoc heaps (constructor local scope).
  uint32_t rtvIndex_ = UINT32_MAX;
  uint32_t dsvIndex_ = UINT32_MAX;
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};

  // Cached descriptor table GPU handles
  // These are set by bindTexture/bindSamplerState and used in drawIndexed
  // to avoid invalidation by multiple SetDescriptorHeaps calls
  D3D12_GPU_DESCRIPTOR_HANDLE cachedTextureGpuHandle_{};
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSamplerGpuHandle_{};
};

} // namespace igl::d3d12
