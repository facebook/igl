/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/Buffer.h>
#include <igl/Device.h>
#include <igl/Shader.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

class PlatformDevice;

/// @brief Implements the igl::IDevice interface for DirectX 12
class Device final : public IDevice {
 public:
  explicit Device(std::unique_ptr<D3D12Context> ctx);
  ~Device() override = default;

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

  D3D12Context& getD3D12Context() {
    return *ctx_;
  }
  [[nodiscard]] const D3D12Context& getD3D12Context() const {
    return *ctx_;
  }

 private:
  std::unique_ptr<D3D12Context> ctx_;
  std::unique_ptr<PlatformDevice> platformDevice_;
  size_t drawCount_ = 0;
  size_t shaderCompilationCount_ = 0;
};

} // namespace igl::d3d12
