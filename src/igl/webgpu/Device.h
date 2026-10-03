/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <ldrutils/lutils/Pool.h>
#include <memory>
#include <igl/CommandEncoder.h>
#include <igl/Device.h>
#include <igl/webgpu/BindLayouts.h>
#include <igl/webgpu/DeviceFeatureSet.h>
#include <igl/webgpu/PlatformDevice.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

/// @brief Implements the igl::IDevice interface for WebGPU
class Device final : public IDevice {
 public:
  explicit Device(std::unique_ptr<WebGPUContext> ctx);
  ~Device() override;

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
  [[nodiscard]] std::shared_ptr<IDepthStencilState> createDepthStencilState(
      const DepthStencilStateDesc& desc,
      Result* IGL_NULLABLE outResult) const override;
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
  [[nodiscard]] std::shared_ptr<ITimestampQueries> createTimestampQueries(
      uint32_t maxTimestamps,
      Result* IGL_NULLABLE outResult) const noexcept override;
  [[nodiscard]] std::shared_ptr<IVertexInputState> createVertexInputState(
      const VertexInputStateDesc& desc,
      Result* IGL_NULLABLE outResult) const override;
  [[nodiscard]] std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                                Result* IGL_NULLABLE
                                                                    outResult) override;
  [[nodiscard]] base::IFramebufferInterop* IGL_NULLABLE
  createFramebufferInterop(const base::FramebufferInteropDesc& desc) override;

  // Pipelines
  [[nodiscard]] std::shared_ptr<IComputePipelineState> createComputePipeline(
      const ComputePipelineDesc& desc,
      Result* IGL_NULLABLE outResult) const override;
  [[nodiscard]] std::shared_ptr<IRenderPipelineState> createRenderPipeline(
      const RenderPipelineDesc& desc,
      Result* IGL_NULLABLE outResult) const override;

  // Shaders
  [[nodiscard]] std::unique_ptr<IShaderLibrary> createShaderLibrary(const ShaderLibraryDesc& desc,
                                                                    Result* IGL_NULLABLE
                                                                        outResult) const override;
  [[nodiscard]] std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const override;
  [[nodiscard]] std::unique_ptr<IShaderStages> createShaderStages(const ShaderStagesDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const override;

  // Platform-specific extensions
  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override;
  [[nodiscard]] void* IGL_NULLABLE getNativeDevice() const override;
  [[nodiscard]] bool isDeviceLost() const noexcept override;

  // ICapabilities
  [[nodiscard]] bool hasFeature(DeviceFeatures feature) const override;
  [[nodiscard]] bool hasRequirement(DeviceRequirement requirement) const override;
  bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const override;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat format) const override;
  [[nodiscard]] ShaderVersion getShaderVersion() const override;
  [[nodiscard]] BackendVersion getBackendVersion() const override;

  // Device Statistics
  [[nodiscard]] size_t getCurrentDrawCount() const override;
  [[nodiscard]] size_t getShaderCompilationCount() const override;

  [[nodiscard]] BackendType getBackendType() const override {
    return BackendType::WebGPU;
  }

  [[nodiscard]] NormalizedZRange getNormalizedZRange() const override {
    return NormalizedZRange::ZeroToOne;
  }

  [[nodiscard]] WebGPUContext& getContext() const noexcept {
    return *ctx_;
  }
  [[nodiscard]] const DeviceFeatureSet& getDeviceFeatureSet() const noexcept {
    return deviceFeatureSet_;
  }
  /// Draws of submitted command buffers; getCurrentDrawCount() reports the total.
  void addDrawCount(size_t count) noexcept {
    drawCount_.fetch_add(count, std::memory_order_relaxed);
  }
  [[nodiscard]] BindLayoutCache& getBindLayoutCache() const noexcept {
    return bindLayoutCache_;
  }
  /// BindGroup-API records; encoders bind their resources slot by slot.
  [[nodiscard]] const BindGroupTextureDesc* IGL_NULLABLE
  getBindGroupTextureDesc(BindGroupTextureHandle handle) const {
    return bindGroupTexturesPool_.get(handle);
  }
  [[nodiscard]] const BindGroupBufferDesc* IGL_NULLABLE
  getBindGroupBufferDesc(BindGroupBufferHandle handle) const {
    return bindGroupBuffersPool_.get(handle);
  }

 private:
  /// True (with Result::Code::DeviceLost) once the device is lost; create calls then return null.
  [[nodiscard]] bool failIfLost(Result* IGL_NULLABLE outResult) const noexcept;

  std::unique_ptr<WebGPUContext> ctx_;
  PlatformDevice platformDevice_;
  DeviceFeatureSet deviceFeatureSet_;
  mutable BindLayoutCache bindLayoutCache_;
  ldr::Pool<BindGroupTextureTag, BindGroupTextureDesc> bindGroupTexturesPool_;
  ldr::Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool_;
  mutable size_t shaderCompilationCount_ = 0;
  std::atomic<size_t> drawCount_ = 0;
};

} // namespace igl::webgpu
