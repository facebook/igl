/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12ResourcesBinder.h>
#include <igl/d3d12/RenderPipelineState.h>

namespace igl::d3d12 {

class CommandBuffer;
class Texture;

/**
 * @brief D3D12 implementation of render command encoder
 *
 * RENDER PASS API:
 * ================
 * With Device::isRenderPassApiEnabled() and ID3D12GraphicsCommandList4, begin() only describes the
 * render pass and the encoder records BeginRenderPass()/EndRenderPass(), so LoadAction and
 * StoreAction reach the driver. Otherwise it clears and binds the render targets itself.
 * - BeginRenderPass() waits for the first draw. A pass without draws begins in endEncoding() only
 *   if it has a clear or a resolve to record. Resource barriers are illegal inside a pass, so the
 *   barriers of bindTexture() have to precede it.
 * - Because of that, sampled textures start readable (Device::createTexture()) and producers leave
 *   them readable. endEncoding() moves sampled color attachments and resolve targets to
 *   ALL_SHADER_RESOURCE and a sampled depth attachment to DEPTH_READ | ALL_SHADER_RESOURCE, the
 *   compute encoder does the same for the textures it writes, and generateMipmap() leaves every
 *   subresource readable. A resolve into the swapchain back buffer ends in PRESENT instead. Binding
 *   a readable texture records nothing. Textures written some other way, such as outside IGL, must
 *   be declared in `Dependencies` or be bound before the first draw.
 * - A texture bound after the first draw that still needs a barrier is a fallback: it asserts in
 *   debug builds, ends the pass early, and the next draw continues it. Depth and stencil default to
 *   StoreAction::DontCare, so that split loses them, and attachments with MsaaResolve lose their
 *   multisampled contents, unless the RenderPassDesc stores them.
 * - Store preserves an attachment, DontCare discards it, and MsaaResolve resolves into the resolve
 *   texture without storing the multisampled contents. Depth resolves after the pass.
 * - Memoryless attachments are not transitioned for sampling when the pass ends.
 * - An encoder has to be ended, or destroyed, before its command buffer is destroyed. The command
 *   buffer remembers the encoder that began the open pass, and only that encoder ends it when it is
 *   destroyed without endEncoding().
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
  ~RenderCommandEncoder() override;

  // Initialize encoder and setup render targets
  // IMPORTANT: Must be called exactly once after construction by
  // CommandBuffer::createRenderCommandEncoder. Calling multiple times will result in resource leaks
  // and undefined behavior. Debug builds will assert if called more than once.
  // `dependencies` only matters with the render pass API; see the class comment.
  void begin(const RenderPassDesc& renderPass, const Dependencies& dependencies);

  void endEncoding() override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;
  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  void bindVertexBuffer(uint32_t index,
                        IBuffer& buffer,
                        size_t bufferOffset,
                        size_t attributeStride) override;
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
  void drawMeshTasks(const Dimensions& threadgroupsPerGrid,
                     const Dimensions& threadsPerTaskThreadgroup,
                     const Dimensions& threadsPerMeshThreadgroup) override;
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
  void setCullMode(CullMode cullMode) override;
  void setDepthBias(float depthBias, float slopeScale, float clamp) override;
  void setFrontFacingWinding(WindingMode frontFaceWinding) override;

  // ICommandEncoder interface
  void pushDebugGroupLabel(const char* label, const Color& color) const override;
  void insertDebugEventLabel(const char* label, const Color& color) const override;
  void popDebugGroupLabel() const override;

  // Additional IRenderCommandEncoder interface
  void bindBuffer(uint32_t index,
                  uint8_t target,
                  IBuffer* buffer,
                  size_t bufferOffset = 0,
                  size_t bufferSize = 0) override;
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

  // Describes color attachment `index` for BeginRenderPass(). `arraySlice` is the slice of the
  // render target view, with the cube face and layer included; a resolve writes the same slice of
  // the resolve texture.
  void addRenderPassColorTarget(size_t index,
                                const RenderPassDesc::AttachmentDesc* IGL_NULLABLE attachmentDesc,
                                Texture& texture,
                                const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc,
                                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                                uint32_t arraySlice);
  // Begins the render pass unless it is already open; the first begin uses the accesses from the
  // RenderPassDesc and every later one continues the contents.
  void ensureRenderPassBegun();
  // Ends the open render pass early when binding `texture` needs a resource barrier.
  void splitRenderPassIfBarrierNeeded(ITexture* IGL_NULLABLE texture);
  // Whether a pass without any draw still has an effect: clears and resolves do, everything else
  // leaves the attachments as they were.
  [[nodiscard]] bool hasRenderPassWorkWithoutDraws() const;
  // After the pass, moves `texture` to `state` when it can be sampled. A bind inside a later pass
  // cannot record that barrier.
  void leaveReadable(Texture* IGL_NULLABLE texture, D3D12_RESOURCE_STATES state);
  // After a resolve wrote `texture`: the swapchain back buffer moves to PRESENT, any other texture
  // is left readable like leaveReadable() does.
  void leaveResolveTarget(Texture* IGL_NULLABLE texture);
  // Viewport, scissor, stencil reference and blend factor are cached and recorded again after
  // every BeginRenderPass(). They are recorded right away unless the pass has yet to begin.
  [[nodiscard]] bool recordsDynamicStateNow() const;
  void applyViewport(const D3D12_VIEWPORT& viewport);
  void applyScissor(const D3D12_RECT& scissor);

  // Render pass API state; see the class comment. Only used when `useRenderPass_`.
  bool useRenderPass_ = false;
  // begin() produced a description that BeginRenderPass() can use.
  bool renderPassPrepared_ = false;
  uint32_t renderPassSegmentCount_ = 0;
  UINT numRenderPassTargets_ = 0;
  std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT>
      renderPassTargets_ = {};
  // The resolve ending accesses point into this until the pass ends.
  std::array<D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS,
             D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT>
      resolveSubresources_ = {};
  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC renderPassDepthStencil_ = {};
  bool hasRenderPassDepthStencil_ = false;
  // Per framebuffer color attachment: its resolve texture is written by the pass, or resolved with
  // ResolveSubresource() after it for formats that cannot resolve in the pass.
  std::array<bool, IGL_COLOR_ATTACHMENTS_MAX> colorResolvedInPass_ = {};
  std::array<bool, IGL_COLOR_ATTACHMENTS_MAX> colorResolvedAfterPass_ = {};
  bool depthResolvedAfterPass_ = false;

  D3D12_VIEWPORT cachedViewport_ = {};
  D3D12_RECT cachedScissor_ = {};
  uint32_t cachedStencilReference_ = 0;
  std::array<float, 4> cachedBlendFactor_ = {};
  bool hasCachedViewport_ = false;
  bool hasCachedScissor_ = false;
  bool hasCachedStencilReference_ = false;
  bool hasCachedBlendFactor_ = false;

  // Keeps the DSV heap alive when there is no DescriptorHeapManager.
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> dsvFallbackHeap_;
};

} // namespace igl::d3d12
