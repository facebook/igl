/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Metal/Metal.h>
#include <igl/CommandEncoder.h>
#include <igl/Device.h>
#include <igl/metal/DeviceFeatureSet.h>
#include <igl/metal/DeviceStatistics.h>
#include <igl/metal/PlatformDevice.h>

namespace igl::metal {

class BufferSynchronizationManager;

class Device : public IDevice {
  friend class HWDevice;

 public:
  explicit Device(id<MTLDevice> IGL_NONNULL device);
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
  std::shared_ptr<ICommandQueue> createCommandQueue(const CommandQueueDesc& desc,
                                                    Result* IGL_NULLABLE
                                                        outResult) noexcept override;

  // Resources
  std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                        Result* IGL_NULLABLE outResult) const noexcept override;
  std::shared_ptr<IDepthStencilState> createDepthStencilState(const DepthStencilStateDesc& desc,
                                                              Result* IGL_NULLABLE
                                                                  outResult) const override;
  std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                    Result* IGL_NULLABLE outResult) const override;
  std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                          Result* IGL_NULLABLE outResult) const noexcept override;
  std::shared_ptr<ITexture> createTextureView(std::shared_ptr<ITexture> texture,
                                              const TextureViewDesc& desc,
                                              Result* IGL_NULLABLE
                                                  outResult) const noexcept override;

  std::shared_ptr<ITimer> createTimer(Result* IGL_NULLABLE outResult) const noexcept override;

  std::shared_ptr<IVertexInputState> createVertexInputState(const VertexInputStateDesc& desc,
                                                            Result* IGL_NULLABLE
                                                                outResult) const override;

  // Pipelines
  std::shared_ptr<IComputePipelineState> createComputePipeline(const ComputePipelineDesc& desc,
                                                               Result* IGL_NULLABLE
                                                                   outResult) const override;
  std::shared_ptr<IRenderPipelineState> createRenderPipeline(const RenderPipelineDesc& desc,
                                                             Result* IGL_NULLABLE
                                                                 outResult) const override;

  // Shaders
  std::unique_ptr<IShaderLibrary> createShaderLibrary(const ShaderLibraryDesc& desc,
                                                      Result* IGL_NULLABLE
                                                          outResult) const override;

  std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                    Result* IGL_NULLABLE outResult) const override;

  std::unique_ptr<IShaderStages> createShaderStages(const ShaderStagesDesc& desc,
                                                    Result* IGL_NULLABLE outResult) const override;

  // Platform-specific extensions
  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override;

  IGL_INLINE id<MTLDevice> IGL_NONNULL get() const {
    return device_;
  }

  // ICapabilities
  [[nodiscard]] bool isAppleGpu() const;
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

  [[nodiscard]] size_t getGPUMemoryUsage() const override;

  [[nodiscard]] BackendType getBackendType() const override {
    return BackendType::Metal;
  }

  [[nodiscard]] NormalizedZRange getNormalizedZRange() const override {
    return NormalizedZRange::ZeroToOne;
  }

  static MTLStorageMode toMTLStorageMode(ResourceStorage storage);
  static MTLResourceOptions toMTLResourceStorageMode(ResourceStorage storage);

 public:
  Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool_;
  Pool<BindGroupTextureTag, BindGroupTextureDesc> bindGroupTexturesPool_;

 private:
  std::unique_ptr<IBuffer> createRingBuffer(const BufferDesc& desc,
                                            Result* IGL_NULLABLE outResult) const noexcept;

  std::unique_ptr<IBuffer> createBufferNoCopy(const BufferDesc& desc,
                                              Result* IGL_NULLABLE outResult) const;

  id<MTLDevice> IGL_NONNULL device_;
  PlatformDevice platformDevice_;

  DeviceFeatureSet deviceFeatureSet_;
  std::shared_ptr<BufferSynchronizationManager> bufferSyncManager_;
  mutable DeviceStatistics deviceStatistics_;
};

} // namespace igl::metal
