/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/sentinel/PlatformDevice.h>
#include <igl/Device.h>

namespace iglu::sentinel {

/**
 * Sentinel Device intended for safe use where access to a real device is not available.
 * Use cases include returning a reference to a device from a raw pointer when a valid device is not
 * available.
 * All methods return nullptr, the default value or an error.
 */
class Device final : public igl::IDevice {
 public:
  explicit Device(bool shouldAssert = true);

  [[nodiscard]] igl::Holder<igl::BindGroupTextureHandle> createBindGroup(
      const igl::BindGroupTextureDesc& desc,
      const igl::IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
      igl::Result* IGL_NULLABLE outResult) final;
  [[nodiscard]] igl::Holder<igl::BindGroupBufferHandle> createBindGroup(
      const igl::BindGroupBufferDesc& desc,
      igl::Result* IGL_NULLABLE outResult) final;
  void destroy(igl::BindGroupTextureHandle handle) final;
  void destroy(igl::BindGroupBufferHandle handle) final;
  void destroy(igl::SamplerHandle handle) final;

  [[nodiscard]] bool hasFeature(igl::DeviceFeatures feature) const final;
  [[nodiscard]] bool hasRequirement(igl::DeviceRequirement requirement) const final;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      igl::TextureFormat format) const final;
  [[nodiscard]] bool getFeatureLimits(igl::DeviceFeatureLimits featureLimits,
                                      size_t& result) const final;
  [[nodiscard]] igl::ShaderVersion getShaderVersion() const final;
  [[nodiscard]] igl::BackendVersion getBackendVersion() const final;

  [[nodiscard]] std::shared_ptr<igl::ICommandQueue> createCommandQueue(
      const igl::CommandQueueDesc& desc,
      igl::Result* IGL_NULLABLE outResult) final;
  [[nodiscard]] std::unique_ptr<igl::IBuffer> createBuffer(const igl::BufferDesc& desc,
                                                           igl::Result* IGL_NULLABLE
                                                               outResult) const noexcept final;
  [[nodiscard]] std::shared_ptr<igl::IDepthStencilState> createDepthStencilState(
      const igl::DepthStencilStateDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] std::shared_ptr<igl::ISamplerState> createSamplerState(
      const igl::SamplerStateDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] std::shared_ptr<igl::ITexture> createTexture(const igl::TextureDesc& desc,
                                                             igl::Result* IGL_NULLABLE
                                                                 outResult) const noexcept final;
  [[nodiscard]] std::shared_ptr<igl::IVertexInputState> createVertexInputState(
      const igl::VertexInputStateDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] std::shared_ptr<igl::IComputePipelineState> createComputePipeline(
      const igl::ComputePipelineDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] std::shared_ptr<igl::IRenderPipelineState> createRenderPipeline(
      const igl::RenderPipelineDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] std::shared_ptr<igl::IShaderModule> createShaderModule(
      const igl::ShaderModuleDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] std::shared_ptr<igl::IFramebuffer> createFramebuffer(
      const igl::FramebufferDesc& desc,
      igl::Result* IGL_NULLABLE outResult) final;
  [[nodiscard]] const igl::IPlatformDevice& getPlatformDevice() const noexcept final;
  [[nodiscard]] bool verifyScope() final;
  [[nodiscard]] igl::BackendType getBackendType() const final;
  [[nodiscard]] size_t getCurrentDrawCount() const final;
  [[nodiscard]] std::unique_ptr<igl::IShaderLibrary> createShaderLibrary(
      const igl::ShaderLibraryDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  void updateSurface(void* IGL_NONNULL nativeWindowType) final;
  [[nodiscard]] std::unique_ptr<igl::IShaderStages> createShaderStages(
      const igl::ShaderStagesDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;

 private:
  PlatformDevice platformDevice_;
  [[maybe_unused]] bool shouldAssert_;
};

} // namespace iglu::sentinel
