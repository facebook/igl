/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <igl/Buffer.h>
#include <igl/CommandEncoder.h>
#include <igl/Device.h>
#include <igl/Shader.h>
#include <igl/Common.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/D3D12ImmediateCommands.h>  // T07: For IFenceProvider interface

namespace igl::d3d12 {

class PlatformDevice;
class UploadRingBuffer;
class SamplerState;  // Forward declaration for sampler cache
class D3D12StagingDevice;  // T07: Forward declaration

/// @brief Implements the igl::IDevice interface for DirectX 12
class Device final : public IDevice, public IFenceProvider {
 public:
  explicit Device(std::unique_ptr<D3D12Context> ctx);
  ~Device() override;

  // BindGroups
  [[nodiscard]] Holder<BindGroupTextureHandle> createBindGroup(
      const BindGroupTextureDesc& desc,
      const IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
      Result* IGL_NULLABLE outResult) override;
  [[nodiscard]] Holder<BindGroupBufferHandle> createBindGroup(
      const BindGroupBufferDesc& desc,
      Result* IGL_NULLABLE outResult) override;
  void destroy(BindGroupTextureHandle handle) override;
  void destroy(BindGroupBufferHandle handle) override;
  void destroy(SamplerHandle handle) override;

  // Command Queue
  [[nodiscard]] std::shared_ptr<ICommandQueue> createCommandQueue(
      const CommandQueueDesc& desc,
      Result* IGL_NULLABLE outResult) noexcept override;

  // Resources
  [[nodiscard]] std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                                      Result* IGL_NULLABLE
                                                          outResult) const noexcept override;

  // T19: Non-const helper for createBuffer - handles upload operations that mutate internal state
  [[nodiscard]] std::unique_ptr<IBuffer> createBufferImpl(const BufferDesc& desc,
                                                          Result* IGL_NULLABLE outResult) noexcept;

  [[nodiscard]] std::shared_ptr<IDepthStencilState> createDepthStencilState(
      const DepthStencilStateDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  [[nodiscard]] std::unique_ptr<IShaderStages> createShaderStages(
      const ShaderStagesDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  [[nodiscard]] std::shared_ptr<ISamplerState> createSamplerState(
      const SamplerStateDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  [[nodiscard]] std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                                        Result* IGL_NULLABLE
                                                            outResult) const noexcept override;

  [[nodiscard]] std::shared_ptr<ITexture> createTextureView(
      std::shared_ptr<ITexture> texture,
      const TextureViewDesc& desc,
      Result* IGL_NULLABLE outResult) const noexcept override;

  [[nodiscard]] std::shared_ptr<ITimer> createTimer(
      Result* IGL_NULLABLE outResult) const noexcept override;

  [[nodiscard]] std::shared_ptr<IVertexInputState> createVertexInputState(
      const VertexInputStateDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  // Pipelines
  [[nodiscard]] std::shared_ptr<IComputePipelineState> createComputePipeline(
      const ComputePipelineDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  [[nodiscard]] std::shared_ptr<IRenderPipelineState> createRenderPipeline(
      const RenderPipelineDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  // Shader library and modules
  [[nodiscard]] std::unique_ptr<IShaderLibrary> createShaderLibrary(
      const ShaderLibraryDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  [[nodiscard]] std::shared_ptr<IShaderModule> createShaderModule(
      const ShaderModuleDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  // Framebuffer
  [[nodiscard]] std::shared_ptr<IFramebuffer> createFramebuffer(
      const FramebufferDesc& desc,
      Result* IGL_NULLABLE outResult) override;

  // Capabilities
  [[nodiscard]] const IPlatformDevice& getPlatformDevice() const noexcept override;

  [[nodiscard]] bool hasFeature(DeviceFeatures feature) const override;
  [[nodiscard]] bool hasRequirement(DeviceRequirement requirement) const override;
  [[nodiscard]] bool getFeatureLimits(DeviceFeatureLimits featureLimits,
                                     size_t& result) const override;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat format) const override;
  [[nodiscard]] ShaderVersion getShaderVersion() const override;
  [[nodiscard]] BackendVersion getBackendVersion() const override;

  [[nodiscard]] BackendType getBackendType() const override;

  [[nodiscard]] size_t getCurrentDrawCount() const override;
  [[nodiscard]] size_t getShaderCompilationCount() const override;

  void incrementDrawCount(size_t n) { drawCount_ += n; }

  D3D12Context& getD3D12Context() {
    return *ctx_;
  }
  [[nodiscard]] const D3D12Context& getD3D12Context() const {
    return *ctx_;
  }

  // Bind group accessors for RenderCommandEncoder
  [[nodiscard]] const BindGroupTextureDesc* getBindGroupTextureDesc(BindGroupTextureHandle handle) const {
    return bindGroupTexturesPool_.get(handle);
  }
  [[nodiscard]] const BindGroupBufferDesc* getBindGroupBufferDesc(BindGroupBufferHandle handle) const {
    return bindGroupBuffersPool_.get(handle);
  }

  // Device capabilities accessors (P2_DX12-018)
  [[nodiscard]] const D3D12_FEATURE_DATA_D3D12_OPTIONS& getDeviceOptions() const {
    return deviceOptions_;
  }
  [[nodiscard]] const D3D12_FEATURE_DATA_D3D12_OPTIONS1& getDeviceOptions1() const {
    return deviceOptions1_;
  }
  [[nodiscard]] D3D12_RESOURCE_BINDING_TIER getResourceBindingTier() const {
    return deviceOptions_.ResourceBindingTier;
  }

  void processCompletedUploads();
  void trackUploadBuffer(igl::d3d12::ComPtr<ID3D12Resource> buffer, UINT64 fenceValue);

  // Command allocator pool access for upload operations (P0_DX12-005)
  igl::d3d12::ComPtr<ID3D12CommandAllocator> getUploadCommandAllocator();
  void returnUploadCommandAllocator(igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator,
                                     UINT64 fenceValue);
  ID3D12Fence* getUploadFence() const { return uploadFence_.Get(); }
  UINT64 getNextUploadFenceValue() { return ++uploadFenceValue_; }
  Result waitForUploadFence(UINT64 fenceValue) const;  // T05: Wait for upload to complete

  // T07: IFenceProvider implementation (shared fence timeline)
  uint64_t getNextFenceValue() override { return getNextUploadFenceValue(); }

  // Upload ring buffer access (P1_DX12-009)
  UploadRingBuffer* getUploadRingBuffer() const { return uploadRingBuffer_.get(); }

  // Check for device removal and return error Result if detected (P1_DX12-006)
  [[nodiscard]] Result checkDeviceRemoval() const;

  // Query if device has been lost (T03: Result-based error handling)
  [[nodiscard]] bool isDeviceLost() const { return deviceLost_; }

  // Sampler cache statistics (C-005)
  struct SamplerCacheStats {
    size_t cacheHits = 0;
    size_t cacheMisses = 0;
    size_t activeSamplers = 0;    // Currently cached samplers
    float hitRate = 0.0f;         // Hit rate percentage
  };
  [[nodiscard]] SamplerCacheStats getSamplerCacheStats() const;

  // I-003: Query maximum MSAA sample count for a specific format
  // Returns 1 if format does not support MSAA
  [[nodiscard]] uint32_t getMaxMSAASamplesForFormat(TextureFormat format) const;

 private:
  // Validate device limits against actual device capabilities (P2_DX12-018)
  void validateDeviceLimits();

  // Alignment validation helpers (B-005)
  bool validateMSAAAlignment(const TextureDesc& desc, Result* IGL_NULLABLE outResult) const;
  bool validateTextureAlignment(const D3D12_RESOURCE_DESC& resourceDesc,
                                 uint32_t sampleCount, Result* IGL_NULLABLE outResult) const;
  bool validateBufferAlignment(size_t bufferSize, bool isUniform) const;

  // Alignment constants (B-005)
  static constexpr size_t MSAA_ALIGNMENT = 65536;  // 64KB for MSAA textures
  static constexpr size_t BUFFER_ALIGNMENT = 256;   // 256 bytes for constant buffers
  static constexpr size_t DEFAULT_TEXTURE_ALIGNMENT = 65536;  // 64KB default for textures

  // Root signature caching (P0_DX12-002)
  size_t hashRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc) const;
  igl::d3d12::ComPtr<ID3D12RootSignature> getOrCreateRootSignature(
      const D3D12_ROOT_SIGNATURE_DESC& desc,
      Result* IGL_NULLABLE outResult) const;

  // Device capabilities (P2_DX12-018)
  D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 deviceOptions1_ = {};

  std::unique_ptr<D3D12Context> ctx_;
  std::unique_ptr<PlatformDevice> platformDevice_;
  size_t drawCount_ = 0;
  size_t shaderCompilationCount_ = 0;

  // Bind group pools
  Pool<BindGroupTextureTag, BindGroupTextureDesc> bindGroupTexturesPool_;
  Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool_;

  // T19: Upload tracking state (non-mutable, mutated only from non-const paths)
  // Modified by: createBufferImpl, Buffer::upload, Texture::upload via non-const Device references
  // Synchronized via pendingUploadsMutex_ for thread-safe access
  struct PendingUpload {
    UINT64 fenceValue = 0;
    igl::d3d12::ComPtr<ID3D12Resource> resource;
  };
  std::mutex pendingUploadsMutex_;
  std::vector<PendingUpload> pendingUploads_;

  // Command allocator pool for upload operations (P0_DX12-005, H-004, B-008)
  // Tracks command allocators with fence values to prevent reuse before GPU completion
  // H-004: Pool capped at 64 allocators to prevent memory leaks
  // B-008: Increased to 256 allocators with enhanced statistics
  // T19: Non-mutable, synchronized via commandAllocatorPoolMutex_
  struct TrackedCommandAllocator {
    igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator;
    UINT64 fenceValue = 0;  // Fence value when last used
  };
  std::mutex commandAllocatorPoolMutex_;
  std::vector<TrackedCommandAllocator> commandAllocatorPool_;
  size_t totalCommandAllocatorsCreated_ = 0;  // H-004: Track total allocators created
  size_t peakPoolSize_ = 0;  // B-008: Track peak pool size
  size_t totalAllocatorReuses_ = 0;  // B-008: Track reuse count for statistics
  igl::d3d12::ComPtr<ID3D12Fence> uploadFence_;
  UINT64 uploadFenceValue_ = 0;  // T19: Incremented only from non-const methods
  // T05: No shared event - waitForUploadFence creates per-call events for thread safety

  // PSO caching (P0_DX12-001)
  mutable std::unordered_map<size_t, igl::d3d12::ComPtr<ID3D12PipelineState>> graphicsPSOCache_;
  mutable std::unordered_map<size_t, igl::d3d12::ComPtr<ID3D12PipelineState>> computePSOCache_;
  mutable std::mutex psoCacheMutex_;  // H-013: Thread-safe PSO cache access
  mutable size_t graphicsPSOCacheHits_ = 0;
  mutable size_t graphicsPSOCacheMisses_ = 0;
  mutable size_t computePSOCacheHits_ = 0;
  mutable size_t computePSOCacheMisses_ = 0;

  // Helper functions for PSO hashing
  size_t hashRenderPipelineDesc(const RenderPipelineDesc& desc) const;
  size_t hashComputePipelineDesc(const ComputePipelineDesc& desc) const;

  // Root signature cache (P0_DX12-002)
  mutable std::unordered_map<size_t, igl::d3d12::ComPtr<ID3D12RootSignature>> rootSignatureCache_;
  mutable std::mutex rootSignatureCacheMutex_;
  mutable size_t rootSignatureCacheHits_ = 0;
  mutable size_t rootSignatureCacheMisses_ = 0;

  // Sampler state cache for deduplication (C-005: Mitigate 2048 sampler heap limit)
  // Use weak_ptr so cache entries are automatically removed when all references destroyed
  mutable std::unordered_map<size_t, std::weak_ptr<SamplerState>> samplerCache_;
  mutable std::mutex samplerCacheMutex_;
  mutable size_t samplerCacheHits_ = 0;
  mutable size_t samplerCacheMisses_ = 0;

  // Upload ring buffer for streaming resources (P1_DX12-009)
  std::unique_ptr<UploadRingBuffer> uploadRingBuffer_;

  // T07: Centralized immediate commands and staging device
  std::unique_ptr<D3D12ImmediateCommands> immediateCommands_;
  std::unique_ptr<D3D12StagingDevice> stagingDevice_;

  // T03: Device lost flag and reason for fatal error handling (atomic for thread-safe access)
  mutable std::atomic<bool> deviceLost_{false};
  mutable std::string deviceLostReason_;  // Cached reason for diagnostics

  // I-007: GPU timestamp frequency for timer queries (Hz)
  // Queried once during device initialization via ID3D12CommandQueue::GetTimestampFrequency()
  // Used by Timer to convert GPU ticks to nanoseconds
  uint64_t gpuTimestampFrequencyHz_ = 0;

 public:
  // T07/T26: Shared staging infrastructure for upload/readback operations
  // Used by Buffer, Texture, Framebuffer, CommandBuffer for centralized resource management
  [[nodiscard]] D3D12ImmediateCommands* getImmediateCommands() const {
    return immediateCommands_.get();
  }
  [[nodiscard]] D3D12StagingDevice* getStagingDevice() const {
    return stagingDevice_.get();
  }
};

} // namespace igl::d3d12
