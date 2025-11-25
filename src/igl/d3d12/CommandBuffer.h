/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

class Device;

class CommandBuffer final : public ICommandBuffer {
 public:
  CommandBuffer(Device& device, const CommandBufferDesc& desc);
  ~CommandBuffer() override;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const Dependencies& dependencies,
      Result* IGL_NULLABLE outResult) override;

  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  void present(const std::shared_ptr<ITexture>& surface) const override;

  void waitUntilScheduled() override;
  void waitUntilCompleted() override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;

  void copyBuffer(IBuffer& source,
                  IBuffer& destination,
                  uint64_t sourceOffset,
                  uint64_t destinationOffset,
                  uint64_t size) override;
  void copyTextureToBuffer(ITexture& source,
                           IBuffer& destination,
                           uint64_t destinationOffset,
                           uint32_t mipLevel,
                           uint32_t layer) override;

  void begin();
  void end();
  bool isRecording() const { return recording_; }

  ID3D12GraphicsCommandList* getCommandList() const { return commandList_.Get(); }
  D3D12Context& getContext();
  const D3D12Context& getContext() const;
  Device& getDevice() { return device_; }

  size_t getCurrentDrawCount() const { return currentDrawCount_; }
  void incrementDrawCount(size_t count = 1) { currentDrawCount_ += count; }

  // Track transient resources (e.g., push constants buffers) that need to be kept alive
  // until this FRAME completes GPU execution (not just until this command buffer is destroyed)
  void trackTransientBuffer(std::shared_ptr<IBuffer> buffer);
  void trackTransientResource(ID3D12Resource* resource);

  // ============================================================================
  // INTERNAL API: Descriptor Allocation (Transient Descriptor Allocator)
  // ============================================================================
  //
  // These methods are implementation details of the per-frame descriptor heap
  // management system (Strategy 1 in D3D12ResourcesBinder.h).
  //
  // WARNING: Do NOT call these methods directly. Use D3D12ResourcesBinder instead.
  //
  // These methods delegate to D3D12Context::FrameContext to share descriptor heaps
  // across all command buffers in the current frame, ensuring efficient utilization
  // and automatic cleanup at frame boundaries.
  //
  // Access: public for friend class D3D12ResourcesBinder, conceptually private.
  // Returns Result to allow error handling on heap overflow.
  // ============================================================================

  /**
   * @brief Allocate a single CBV/SRV/UAV descriptor from per-frame heap
   * @internal This is an implementation detail - use D3D12ResourcesBinder instead
   */
  Result getNextCbvSrvUavDescriptor(uint32_t* outDescriptorIndex);

  /**
   * @brief Allocate a contiguous range of CBV/SRV/UAV descriptors on a single page
   * @internal This is an implementation detail - use D3D12ResourcesBinder instead
   *
   * This ensures the range can be bound as a single descriptor table.
   * Returns the base descriptor index; descriptors are [baseIndex, baseIndex+count)
   */
  Result allocateCbvSrvUavRange(uint32_t count, uint32_t* outBaseDescriptorIndex);

  /**
   * @brief Get reference to next sampler descriptor index (for increment)
   * @internal This is an implementation detail - use D3D12ResourcesBinder instead
   */
  uint32_t& getNextSamplerDescriptor();

  // Deferred texture-to-buffer copy operations
  // These are recorded during command buffer recording and executed in CommandQueue::submit()
  // AFTER all render/compute commands have been executed by the GPU
  struct DeferredTextureCopy {
    ITexture* source;
    IBuffer* destination;
    uint64_t destinationOffset;
    uint32_t mipLevel;
    uint32_t layer;
  };
  const std::vector<DeferredTextureCopy>& getDeferredTextureCopies() const {
    return deferredTextureCopies_;
  }

  // Whether this command buffer requested a swapchain present via present().
  bool willPresent() const { return willPresent_; }

 private:
  Device& device_;
  igl::d3d12::ComPtr<ID3D12GraphicsCommandList> commandList_;
  // NOTE: Command allocators are now managed per-frame in FrameContext, not per-CommandBuffer
  size_t currentDrawCount_ = 0;
  bool recording_ = false;

  // Scheduling fence infrastructure (separate from completion fence)
  // Used to track when command buffer is submitted to GPU queue (not when GPU completes)
  // D-003: Removed scheduleFenceEvent_ - now using dedicated events per wait operation
  igl::d3d12::ComPtr<ID3D12Fence> scheduleFence_;
  uint64_t scheduleValue_ = 0;

  // Deferred copy operations to execute after command buffer submission
  std::vector<DeferredTextureCopy> deferredTextureCopies_;

  // Tracks whether present(surface) was called on this command buffer.
  // Mutable to allow modification from the logically-const present() override.
  mutable bool willPresent_ = false;

  friend class CommandQueue; // Allow CommandQueue to signal scheduleFence_
};

} // namespace igl::d3d12
