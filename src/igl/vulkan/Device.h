/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Device.h>
#include <igl/Shader.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/VulkanSemaphore.h>
#include <memory>

namespace igl::vulkan {

class VulkanContext;
class VulkanShaderModule;

/// @brief Implements the igl::IDevice interface for Vulkan
class Device final : public IDevice {
 public:
  explicit Device(std::unique_ptr<VulkanContext> ctx);
  ~Device() override;

  [[nodiscard]] Holder<igl::BindGroupTextureHandle> createBindGroup(
      const BindGroupTextureDesc& desc,
      const IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
      Result* IGL_NULLABLE outResult) override;
  [[nodiscard]] Holder<igl::BindGroupBufferHandle> createBindGroup(const BindGroupBufferDesc& desc,
                                                                   Result* IGL_NULLABLE
                                                                       outResult) override;
  void destroy(igl::BindGroupTextureHandle handle) override;
  void destroy(igl::BindGroupBufferHandle handle) override;
  void destroy(igl::SamplerHandle handle) override;

  // Command Queue
  std::shared_ptr<ICommandQueue> createCommandQueue(const CommandQueueDesc& desc,
                                                    Result* IGL_NULLABLE outResult) override;
  // Resources
  std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                        Result* IGL_NULLABLE outResult) const noexcept override;

  std::shared_ptr<IDepthStencilState> createDepthStencilState(const DepthStencilStateDesc& desc,
                                                              Result* IGL_NULLABLE
                                                                  outResult) const override;

  std::unique_ptr<IShaderStages> createShaderStages(const ShaderStagesDesc& desc,
                                                    Result* IGL_NULLABLE outResult) const override;

  std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                    Result* IGL_NULLABLE outResult) const override;
  std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                          Result* IGL_NULLABLE outResult) const noexcept override;

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

  std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                  Result* IGL_NULLABLE outResult) override;

  // Platform-specific extensions
  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override;

  // ICapabilities
  [[nodiscard]] bool hasFeature(DeviceFeatures feature) const override;
  [[nodiscard]] bool hasRequirement(DeviceRequirement requirement) const override;
  bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const override;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat format) const override;
  [[nodiscard]] ShaderVersion getShaderVersion() const override;
  [[nodiscard]] BackendVersion getBackendVersion() const override;

  [[nodiscard]] BackendType getBackendType() const override {
    return BackendType::Vulkan;
  }
  [[nodiscard]] size_t getCurrentDrawCount() const override;

  void setCurrentThread() override;

  VulkanContext& getVulkanContext() {
    return *ctx_;
  }
  [[nodiscard]] const VulkanContext& getVulkanContext() const {
    return *ctx_;
  }

 private:
  std::shared_ptr<VulkanShaderModule> createShaderModule(const void* IGL_NULLABLE data,
                                                         size_t length,
                                                         const std::string& debugName,
                                                         Result* IGL_NULLABLE outResult) const;
  std::shared_ptr<VulkanShaderModule> createShaderModule(ShaderStage stage,
                                                         const char* IGL_NULLABLE source,
                                                         const std::string& debugName,
                                                         Result* IGL_NULLABLE outResult) const;

  std::unique_ptr<VulkanContext> ctx_;

  PlatformDevice platformDevice_;
};

} // namespace igl::vulkan
