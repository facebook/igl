/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Device.h>
#include <igl/Shader.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSemaphore.h>
#include <memory>

namespace igl::vulkan {

class VulkanShaderModule;

/// @brief Implements the igl::IDevice interface for Vulkan
class Device final : public IDevice {
 public:
  explicit Device(std::unique_ptr<VulkanContext> ctx);
  ~Device() override = default;

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
                                                                      outResult) override;
  // Resources
  [[nodiscard]] std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                                      Result* IGL_NULLABLE
                                                          outResult) const noexcept override;

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

  // Shaders
  [[nodiscard]] std::unique_ptr<IShaderLibrary> createShaderLibrary(const ShaderLibraryDesc& desc,
                                                                    Result* IGL_NULLABLE
                                                                        outResult) const override;

  [[nodiscard]] std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const override;

  [[nodiscard]] std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                                Result* IGL_NULLABLE
                                                                    outResult) override;

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

  [[nodiscard]] BackendType getBackendType() const override;
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

  [[nodiscard]] Holder<BindGroupTextureHandle> createBindGroupInternal(
      const BindGroupTextureDesc& desc,
      const IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
      Result* IGL_NULLABLE outResult);
  [[nodiscard]] Holder<BindGroupBufferHandle> createBindGroupInternal(
      const BindGroupBufferDesc& desc,
      Result* IGL_NULLABLE outResult);
  void destroyInternal(BindGroupTextureHandle handle);
  void destroyInternal(BindGroupBufferHandle handle);
  void destroyInternal(SamplerHandle handle);

  // Command Queue
  std::shared_ptr<ICommandQueue> createCommandQueueInternal(const CommandQueueDesc& desc,
                                                            Result* IGL_NULLABLE outResult);
  // Resources
  std::unique_ptr<IBuffer> createBufferInternal(const BufferDesc& desc,
                                                Result* IGL_NULLABLE outResult) const noexcept;

  std::shared_ptr<IDepthStencilState> createDepthStencilStateInternal(
      const DepthStencilStateDesc& desc,
      Result* IGL_NULLABLE outResult) const;

  std::unique_ptr<IShaderStages> createShaderStagesInternal(const ShaderStagesDesc& desc,
                                                            Result* IGL_NULLABLE outResult) const;

  std::shared_ptr<ISamplerState> createSamplerStateInternal(const SamplerStateDesc& desc,
                                                            Result* IGL_NULLABLE outResult) const;
  std::shared_ptr<ITexture> createTextureInternal(const TextureDesc& desc,
                                                  Result* IGL_NULLABLE outResult) const noexcept;

  std::shared_ptr<IVertexInputState> createVertexInputStateInternal(
      const VertexInputStateDesc& desc,
      Result* IGL_NULLABLE outResult) const;

  // Pipelines
  std::shared_ptr<IComputePipelineState> createComputePipelineInternal(
      const ComputePipelineDesc& desc,
      Result* IGL_NULLABLE outResult) const;
  std::shared_ptr<IRenderPipelineState> createRenderPipelineInternal(const RenderPipelineDesc& desc,
                                                                     Result* IGL_NULLABLE
                                                                         outResult) const;

  // Shaders
  std::unique_ptr<IShaderLibrary> createShaderLibraryInternal(const ShaderLibraryDesc& desc,
                                                              Result* IGL_NULLABLE outResult) const;

  std::shared_ptr<IShaderModule> createShaderModuleInternal(const ShaderModuleDesc& desc,
                                                            Result* IGL_NULLABLE outResult) const;

  std::shared_ptr<IFramebuffer> createFramebufferInternal(const FramebufferDesc& desc,
                                                          Result* IGL_NULLABLE outResult);

  // Platform-specific extensions
  [[nodiscard]] const PlatformDevice& getPlatformDeviceInternal() const noexcept;

  // ICapabilities
  [[nodiscard]] bool hasFeatureInternal(DeviceFeatures feature) const;
  [[nodiscard]] bool hasRequirementInternal(DeviceRequirement requirement) const;
  bool getFeatureLimitsInternal(DeviceFeatureLimits featureLimits, size_t& result) const;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilitiesInternal(
      TextureFormat format) const;
  [[nodiscard]] ShaderVersion getShaderVersionInternal() const;
  [[nodiscard]] BackendVersion getBackendVersionInternal() const;

  [[nodiscard]] size_t getCurrentDrawCountInternal() const;

  void setCurrentThreadInternal();

  std::unique_ptr<VulkanContext> ctx_;

  PlatformDevice platformDevice_;
};

/// Inline, passthrough implementations of virtual methods to work around mixing rtti and no-rtti
/// targets on iOS and Android

[[nodiscard]] inline Holder<BindGroupTextureHandle> Device::createBindGroup(
    const BindGroupTextureDesc& desc,
    const IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
    Result* IGL_NULLABLE outResult) {
  return createBindGroupInternal(desc, compatiblePipeline, outResult);
}

[[nodiscard]] inline Holder<BindGroupBufferHandle> Device::createBindGroup(
    const BindGroupBufferDesc& desc,
    Result* IGL_NULLABLE outResult) {
  return createBindGroupInternal(desc, outResult);
}

void inline Device::destroy(BindGroupTextureHandle handle) {
  destroyInternal(handle);
}

void inline Device::destroy(BindGroupBufferHandle handle) {
  destroyInternal(handle);
}

void inline Device::destroy(SamplerHandle handle) {
  destroyInternal(handle);
}

[[nodiscard]] inline std::shared_ptr<ICommandQueue> Device::createCommandQueue(
    const CommandQueueDesc& desc,
    Result* IGL_NULLABLE outResult) {
  return createCommandQueueInternal(desc, outResult);
}

[[nodiscard]] inline std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                                                   Result* IGL_NULLABLE
                                                                       outResult) const noexcept {
  return createBufferInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<IDepthStencilState> Device::createDepthStencilState(
    const DepthStencilStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createDepthStencilStateInternal(desc, outResult);
}

[[nodiscard]] inline std::unique_ptr<IShaderStages> Device::createShaderStages(
    const ShaderStagesDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createShaderStagesInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<ISamplerState> Device::createSamplerState(
    const SamplerStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createSamplerStateInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                                     Result* IGL_NULLABLE
                                                                         outResult) const noexcept {
  return createTextureInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<IVertexInputState> Device::createVertexInputState(
    const VertexInputStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createVertexInputStateInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createComputePipelineInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(
    const RenderPipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createRenderPipelineInternal(desc, outResult);
}

[[nodiscard]] inline std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(
    const ShaderLibraryDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createShaderLibraryInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<IShaderModule> Device::createShaderModule(
    const ShaderModuleDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  return createShaderModuleInternal(desc, outResult);
}

[[nodiscard]] inline std::shared_ptr<IFramebuffer> Device::createFramebuffer(
    const FramebufferDesc& desc,
    Result* IGL_NULLABLE outResult) {
  return createFramebufferInternal(desc, outResult);
}

// Platform-specific extensions
[[nodiscard]] inline const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return getPlatformDeviceInternal();
}

[[nodiscard]] inline bool Device::hasFeature(DeviceFeatures feature) const {
  return hasFeatureInternal(feature);
}

[[nodiscard]] inline bool Device::hasRequirement(DeviceRequirement requirement) const {
  return hasRequirementInternal(requirement);
}

inline bool Device::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  return getFeatureLimitsInternal(featureLimits, result);
}

[[nodiscard]] inline ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(
    TextureFormat format) const {
  return getTextureFormatCapabilitiesInternal(format);
}

[[nodiscard]] inline ShaderVersion Device::getShaderVersion() const {
  return getShaderVersionInternal();
}

[[nodiscard]] inline BackendVersion Device::getBackendVersion() const {
  return getBackendVersionInternal();
}

[[nodiscard]] inline BackendType Device::getBackendType() const {
  return BackendType::Vulkan;
}

[[nodiscard]] inline size_t Device::getCurrentDrawCount() const {
  return getCurrentDrawCountInternal();
}

inline void Device::setCurrentThread() {
  setCurrentThreadInternal();
}

} // namespace igl::vulkan
