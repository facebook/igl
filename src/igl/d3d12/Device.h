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
#include <unordered_map>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandEncoder.h>
#include <igl/Common.h>
#include <igl/Device.h>
#include <igl/Shader.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12AllocatorPool.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/D3D12DeviceCapabilities.h>
#include <igl/d3d12/D3D12ImmediateCommands.h> // For IFenceProvider interface.
#include <igl/d3d12/D3D12PipelineCache.h>
#include <igl/d3d12/D3D12SamplerCache.h>
#include <igl/d3d12/D3D12Telemetry.h>

namespace igl::d3d12 {

class PlatformDevice;
class UploadRingBuffer;
class SamplerState; // Forward declaration for sampler cache
class D3D12StagingDevice; // Forward declaration.

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
  [[nodiscard]] Holder<BindGroupBufferHandle> createBindGroup(const BindGroupBufferDesc& desc,
                                                              Result* IGL_NULLABLE
                                                                  outResult) override;
  void destroy(BindGroupTextureHandle handle) override;
  void destroy(BindGroupBufferHandle handle) override;
  void destroy(SamplerHandle handle) override;

  // Command Queue
  [[nodiscard]] std::shared_ptr<ICommandQueue> createCommandQueue(const CommandQueueDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) noexcept override;

  // Resources
  [[nodiscard]] std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                                      Result* IGL_NULLABLE
                                                          outResult) const noexcept override;

  // Non-const helper for createBuffer; handles upload operations that mutate internal state.
  [[nodiscard]] std::unique_ptr<IBuffer> createBufferImpl(const BufferDesc& desc,
                                                          Result* IGL_NULLABLE outResult) noexcept;

  [[nodiscard]] std::shared_ptr<IDepthStencilState> createDepthStencilState(
      const DepthStencilStateDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  [[nodiscard]] std::unique_ptr<IShaderStages> createShaderStages(const ShaderStagesDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const override;

  [[nodiscard]] std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const override;

  [[nodiscard]] std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                                        Result* IGL_NULLABLE
                                                            outResult) const noexcept override;

  [[nodiscard]] std::shared_ptr<ITexture> createTextureView(std::shared_ptr<ITexture> texture,
                                                            const TextureViewDesc& desc,
                                                            Result* IGL_NULLABLE
                                                                outResult) const noexcept override;

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

  // D3D12-specific: Create PSO variant with substituted formats (for dynamic PSO selection)
  // Called by RenderPipelineState::getPipelineState() to create format variants
  [[nodiscard]] igl::d3d12::ComPtr<ID3D12PipelineState> createPipelineStateVariant(
      const RenderPipelineDesc& desc,
      ID3D12RootSignature* rootSignature,
      Result* IGL_NULLABLE outResult) const;

  // Shader library and modules
  [[nodiscard]] std::unique_ptr<IShaderLibrary> createShaderLibrary(const ShaderLibraryDesc& desc,
                                                                    Result* IGL_NULLABLE
                                                                        outResult) const override;

  [[nodiscard]] std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const override;

  // Framebuffer
  [[nodiscard]] std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                                Result* IGL_NULLABLE
                                                                    outResult) override;

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

  [[nodiscard]] size_t getCurrentDrawCount() const override;
  [[nodiscard]] size_t getShaderCompilationCount() const override;

  [[nodiscard]] BackendType getBackendType() const override;

  [[nodiscard]] void* IGL_NULLABLE getNativeDevice() const override {
    return ctx_->getDevice();
  }

  [[nodiscard]] base::IFramebufferInterop* IGL_NULLABLE
  createFramebufferInterop(const base::FramebufferInteropDesc& desc) override;

  void incrementDrawCount(size_t n) {
    telemetry_.incrementDrawCount(n);
  }

  D3D12Context& getD3D12Context() {
    return *ctx_;
  }
  [[nodiscard]] const D3D12Context& getD3D12Context() const {
    return *ctx_;
  }

  // Bind group accessors for RenderCommandEncoder
  [[nodiscard]] const BindGroupTextureDesc* getBindGroupTextureDesc(
      BindGroupTextureHandle handle) const {
    return bindGroupTexturesPool_.get(handle);
  }
  [[nodiscard]] const BindGroupBufferDesc* getBindGroupBufferDesc(
      BindGroupBufferHandle handle) const {
    return bindGroupBuffersPool_.get(handle);
  }

  // Device capabilities accessors.
  [[nodiscard]] const D3D12_FEATURE_DATA_D3D12_OPTIONS& getDeviceOptions() const {
    return capabilities_.getOptions();
  }
  [[nodiscard]] const D3D12_FEATURE_DATA_D3D12_OPTIONS1& getDeviceOptions1() const {
    return capabilities_.getOptions1();
  }
  [[nodiscard]] D3D12_RESOURCE_BINDING_TIER getResourceBindingTier() const {
    return capabilities_.getResourceBindingTier();
  }

  void processCompletedUploads();
  void trackUploadBuffer(igl::d3d12::ComPtr<ID3D12Resource> buffer, UINT64 fenceValue);

  // Command allocator pool access for upload operations.
  igl::d3d12::ComPtr<ID3D12CommandAllocator> getUploadCommandAllocator();
  void returnUploadCommandAllocator(igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator,
                                    UINT64 fenceValue);
  ID3D12Fence* getUploadFence() const {
    return allocatorPool_.getUploadFence();
  }
  UINT64 getNextUploadFenceValue() {
    return allocatorPool_.getNextUploadFenceValue();
  }
  Result waitForUploadFence(UINT64 fenceValue) const;

  // IFenceProvider implementation (shared fence timeline).
  uint64_t getNextFenceValue() override {
    return getNextUploadFenceValue();
  }

  // Upload ring buffer access.
  UploadRingBuffer* getUploadRingBuffer() const {
    return allocatorPool_.getUploadRingBuffer();
  }

  // Check for device removal and return error Result if detected.
  [[nodiscard]] Result checkDeviceRemoval() const;

  // Query if device has been lost.
  [[nodiscard]] bool isDeviceLost() const {
    return deviceLost_;
  }

  // Sampler cache statistics.
  [[nodiscard]] SamplerCacheStats getSamplerCacheStats() const;

  // Query maximum MSAA sample count for a specific format.
  // Returns 1 if the format does not support MSAA.
  [[nodiscard]] uint32_t getMaxMSAASamplesForFormat(TextureFormat format) const;

 private:
  // Alignment validation helpers.
  bool validateMSAAAlignment(const TextureDesc& desc, Result* IGL_NULLABLE outResult) const;
  bool validateTextureAlignment(const D3D12_RESOURCE_DESC& resourceDesc,
                                uint32_t sampleCount,
                                Result* IGL_NULLABLE outResult) const;
  bool validateBufferAlignment(size_t bufferSize, bool isUniform) const;

  // Alignment constants.
  static constexpr size_t MSAA_ALIGNMENT = 65536; // 64KB for MSAA textures
  static constexpr size_t BUFFER_ALIGNMENT = 256; // 256 bytes for constant buffers
  static constexpr size_t DEFAULT_TEXTURE_ALIGNMENT = 65536; // 64KB default for textures

  D3D12DeviceCapabilities capabilities_;

  std::unique_ptr<D3D12Context> ctx_;
  std::unique_ptr<PlatformDevice> platformDevice_;
  D3D12Telemetry telemetry_;

  // Bind group pools
  Pool<BindGroupTextureTag, BindGroupTextureDesc> bindGroupTexturesPool_;
  Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool_;

  // Upload tracking state (non-mutable, mutated only from non-const paths).
  // Modified by createBufferImpl, Buffer::upload, Texture::upload via non-const Device references
  // and synchronized via pendingUploadsMutex_ for thread-safe access.
  D3D12AllocatorPool allocatorPool_;
  D3D12PipelineCache pipelineCache_;
  D3D12SamplerCache samplerCache_;

  // Device lost flag and reason for fatal error handling (atomic for thread-safe access).
  mutable std::atomic<bool> deviceLost_{false};
  mutable std::string deviceLostReason_; // Cached reason for diagnostics

 public:
  // Shared staging infrastructure for upload/readback operations.
  // Used by Buffer, Texture, Framebuffer, CommandBuffer for centralized resource management.
  [[nodiscard]] D3D12ImmediateCommands* getImmediateCommands() const {
    return allocatorPool_.getImmediateCommands();
  }
  [[nodiscard]] D3D12StagingDevice* getStagingDevice() const {
    return allocatorPool_.getStagingDevice();
  }

  // Access pre-compiled mipmap shaders.
  [[nodiscard]] bool areMipmapShadersAvailable() const {
    return pipelineCache_.mipmapShadersAvailable_;
  }
  [[nodiscard]] const std::vector<uint8_t>& getMipmapVSBytecode() const {
    return pipelineCache_.mipmapVSBytecode_;
  }
  [[nodiscard]] const std::vector<uint8_t>& getMipmapPSBytecode() const {
    return pipelineCache_.mipmapPSBytecode_;
  }
  [[nodiscard]] ID3D12RootSignature* getMipmapRootSignature() const {
    return pipelineCache_.mipmapRootSignature_.Get();
  }
};

} // namespace igl::d3d12
