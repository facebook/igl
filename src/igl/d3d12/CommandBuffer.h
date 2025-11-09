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

  // Descriptor allocation tracking - delegates to frame context to share across ALL command buffers
  // C-001: Changed to return Result for error handling on heap overflow
  Result getNextCbvSrvUavDescriptor(uint32_t* outDescriptorIndex);
  uint32_t& getNextSamplerDescriptor();

 private:
  Device& device_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
  // NOTE: Command allocators are now managed per-frame in FrameContext, not per-CommandBuffer
  size_t currentDrawCount_ = 0;
  bool recording_ = false;

  // Scheduling fence infrastructure (separate from completion fence)
  // Used to track when command buffer is submitted to GPU queue (not when GPU completes)
  Microsoft::WRL::ComPtr<ID3D12Fence> scheduleFence_;
  uint64_t scheduleValue_ = 0;
  HANDLE scheduleFenceEvent_ = nullptr;

  friend class CommandQueue; // Allow CommandQueue to signal scheduleFence_
};

} // namespace igl::d3d12
