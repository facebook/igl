/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/sentinel/PlatformDevice.h>
// The inline virtual forwarders below return std::unique_ptr of these types by value, which
// needs them complete here.
#include <igl/Buffer.h>
#include <igl/Device.h>
#include <igl/Shader.h>

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
      igl::Result* IGL_NULLABLE outResult) noexcept final;
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
  [[nodiscard]] std::shared_ptr<igl::ITexture> createTextureView(
      std::shared_ptr<igl::ITexture> texture,
      const igl::TextureViewDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const noexcept final;
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
  [[nodiscard]] std::shared_ptr<igl::ITimer> createTimer(
      igl::Result* IGL_NULLABLE outResult) const noexcept final;
  [[nodiscard]] const igl::IPlatformDevice& getPlatformDevice() const noexcept final;
  [[nodiscard]] bool verifyScope() final;
  [[nodiscard]] igl::BackendType getBackendType() const final;
  [[nodiscard]] size_t getCurrentDrawCount() const final;
  [[nodiscard]] size_t getShaderCompilationCount() const final;
  [[nodiscard]] std::unique_ptr<igl::IShaderLibrary> createShaderLibrary(
      const igl::ShaderLibraryDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  void updateSurface(void* IGL_NONNULL nativeWindowType) final;
  [[nodiscard]] std::unique_ptr<igl::IShaderStages> createShaderStages(
      const igl::ShaderStagesDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const final;
  [[nodiscard]] void* IGL_NULLABLE getNativeDevice() const override {
    return nullptr;
  }

 private:
  /// Every virtual is an inline forwarder to a non-virtual Impl defined in the .cpp, so this
  /// class has no key function and its vtable and typeinfo stay weak and are emitted per
  /// translation unit. The bodies stay out of line because they use the sentinel assert macro,
  /// which this header does not include.
  [[nodiscard]] igl::Holder<igl::BindGroupTextureHandle> createBindGroupImpl(
      const igl::BindGroupTextureDesc& desc,
      const igl::IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
      igl::Result* IGL_NULLABLE outResult);
  [[nodiscard]] igl::Holder<igl::BindGroupBufferHandle> createBindGroupImpl(
      const igl::BindGroupBufferDesc& desc,
      igl::Result* IGL_NULLABLE outResult);
  void destroyImpl(igl::BindGroupTextureHandle handle);
  void destroyImpl(igl::BindGroupBufferHandle handle);
  void destroyImpl(igl::SamplerHandle handle);
  [[nodiscard]] bool hasFeatureImpl(igl::DeviceFeatures feature) const;
  [[nodiscard]] bool hasRequirementImpl(igl::DeviceRequirement requirement) const;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilitiesImpl(
      igl::TextureFormat format) const;
  [[nodiscard]] bool getFeatureLimitsImpl(igl::DeviceFeatureLimits featureLimits,
                                          size_t& result) const;
  [[nodiscard]] igl::ShaderVersion getShaderVersionImpl() const;
  [[nodiscard]] igl::BackendVersion getBackendVersionImpl() const;
  [[nodiscard]] std::shared_ptr<igl::ICommandQueue> createCommandQueueImpl(
      const igl::CommandQueueDesc& desc,
      igl::Result* IGL_NULLABLE outResult) noexcept;
  [[nodiscard]] std::unique_ptr<igl::IBuffer> createBufferImpl(const igl::BufferDesc& desc,
                                                               igl::Result* IGL_NULLABLE
                                                                   outResult) const noexcept;
  [[nodiscard]] std::shared_ptr<igl::IDepthStencilState> createDepthStencilStateImpl(
      const igl::DepthStencilStateDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  [[nodiscard]] std::shared_ptr<igl::ISamplerState> createSamplerStateImpl(
      const igl::SamplerStateDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  [[nodiscard]] std::shared_ptr<igl::ITexture> createTextureImpl(const igl::TextureDesc& desc,
                                                                 igl::Result* IGL_NULLABLE
                                                                     outResult) const noexcept;
  [[nodiscard]] std::shared_ptr<igl::ITexture> createTextureViewImpl(
      std::shared_ptr<igl::ITexture> texture,
      const igl::TextureViewDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const noexcept;
  [[nodiscard]] std::shared_ptr<igl::IVertexInputState> createVertexInputStateImpl(
      const igl::VertexInputStateDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  [[nodiscard]] std::shared_ptr<igl::IComputePipelineState> createComputePipelineImpl(
      const igl::ComputePipelineDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  [[nodiscard]] std::shared_ptr<igl::IRenderPipelineState> createRenderPipelineImpl(
      const igl::RenderPipelineDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  [[nodiscard]] std::shared_ptr<igl::IShaderModule> createShaderModuleImpl(
      const igl::ShaderModuleDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  [[nodiscard]] std::shared_ptr<igl::IFramebuffer> createFramebufferImpl(
      const igl::FramebufferDesc& desc,
      igl::Result* IGL_NULLABLE outResult);
  [[nodiscard]] std::shared_ptr<igl::ITimer> createTimerImpl(
      igl::Result* IGL_NULLABLE outResult) const noexcept;
  [[nodiscard]] const igl::IPlatformDevice& getPlatformDeviceImpl() const noexcept;
  [[nodiscard]] bool verifyScopeImpl();
  [[nodiscard]] igl::BackendType getBackendTypeImpl() const;
  [[nodiscard]] size_t getCurrentDrawCountImpl() const;
  [[nodiscard]] size_t getShaderCompilationCountImpl() const;
  [[nodiscard]] std::unique_ptr<igl::IShaderLibrary> createShaderLibraryImpl(
      const igl::ShaderLibraryDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;
  void updateSurfaceImpl(void* IGL_NONNULL nativeWindowType);
  [[nodiscard]] std::unique_ptr<igl::IShaderStages> createShaderStagesImpl(
      const igl::ShaderStagesDesc& desc,
      igl::Result* IGL_NULLABLE outResult) const;

  PlatformDevice platformDevice_;
  [[maybe_unused]] bool shouldAssert_;
};

inline igl::Holder<igl::BindGroupTextureHandle> Device::createBindGroup(
    const igl::BindGroupTextureDesc& desc,
    const igl::IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
    igl::Result* IGL_NULLABLE outResult) {
  return createBindGroupImpl(desc, compatiblePipeline, outResult);
}

inline igl::Holder<igl::BindGroupBufferHandle> Device::createBindGroup(
    const igl::BindGroupBufferDesc& desc,
    igl::Result* IGL_NULLABLE outResult) {
  return createBindGroupImpl(desc, outResult);
}

inline void Device::destroy(igl::BindGroupTextureHandle handle) {
  destroyImpl(handle);
}

inline void Device::destroy(igl::BindGroupBufferHandle handle) {
  destroyImpl(handle);
}

inline void Device::destroy(igl::SamplerHandle handle) {
  destroyImpl(handle);
}

inline bool Device::hasFeature(igl::DeviceFeatures feature) const {
  return hasFeatureImpl(feature);
}

inline bool Device::hasRequirement(igl::DeviceRequirement requirement) const {
  return hasRequirementImpl(requirement);
}

inline igl::ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(
    igl::TextureFormat format) const {
  return getTextureFormatCapabilitiesImpl(format);
}

inline bool Device::getFeatureLimits(igl::DeviceFeatureLimits featureLimits, size_t& result) const {
  return getFeatureLimitsImpl(featureLimits, result);
}

inline igl::ShaderVersion Device::getShaderVersion() const {
  return getShaderVersionImpl();
}

inline igl::BackendVersion Device::getBackendVersion() const {
  return getBackendVersionImpl();
}

inline std::shared_ptr<igl::ICommandQueue> Device::createCommandQueue(
    const igl::CommandQueueDesc& desc,
    igl::Result* IGL_NULLABLE outResult) noexcept {
  return createCommandQueueImpl(desc, outResult);
}

inline std::unique_ptr<igl::IBuffer> Device::createBuffer(const igl::BufferDesc& desc,
                                                          igl::Result* IGL_NULLABLE
                                                              outResult) const noexcept {
  return createBufferImpl(desc, outResult);
}

inline std::shared_ptr<igl::IDepthStencilState> Device::createDepthStencilState(
    const igl::DepthStencilStateDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createDepthStencilStateImpl(desc, outResult);
}

inline std::shared_ptr<igl::ISamplerState> Device::createSamplerState(
    const igl::SamplerStateDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createSamplerStateImpl(desc, outResult);
}

inline std::shared_ptr<igl::ITexture> Device::createTexture(const igl::TextureDesc& desc,
                                                            igl::Result* IGL_NULLABLE
                                                                outResult) const noexcept {
  return createTextureImpl(desc, outResult);
}

inline std::shared_ptr<igl::ITexture> Device::createTextureView(
    std::shared_ptr<igl::ITexture> texture,
    const igl::TextureViewDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  return createTextureViewImpl(std::move(texture), desc, outResult);
}

inline std::shared_ptr<igl::IVertexInputState> Device::createVertexInputState(
    const igl::VertexInputStateDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createVertexInputStateImpl(desc, outResult);
}

inline std::shared_ptr<igl::IComputePipelineState> Device::createComputePipeline(
    const igl::ComputePipelineDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createComputePipelineImpl(desc, outResult);
}

inline std::shared_ptr<igl::IRenderPipelineState> Device::createRenderPipeline(
    const igl::RenderPipelineDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createRenderPipelineImpl(desc, outResult);
}

inline std::shared_ptr<igl::IShaderModule> Device::createShaderModule(
    const igl::ShaderModuleDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createShaderModuleImpl(desc, outResult);
}

inline std::shared_ptr<igl::IFramebuffer> Device::createFramebuffer(
    const igl::FramebufferDesc& desc,
    igl::Result* IGL_NULLABLE outResult) {
  return createFramebufferImpl(desc, outResult);
}

inline std::shared_ptr<igl::ITimer> Device::createTimer(
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  return createTimerImpl(outResult);
}

inline const igl::IPlatformDevice& Device::getPlatformDevice() const noexcept {
  return getPlatformDeviceImpl();
}

inline bool Device::verifyScope() {
  return verifyScopeImpl();
}

inline igl::BackendType Device::getBackendType() const {
  return getBackendTypeImpl();
}

inline size_t Device::getCurrentDrawCount() const {
  return getCurrentDrawCountImpl();
}

inline size_t Device::getShaderCompilationCount() const {
  return getShaderCompilationCountImpl();
}

inline std::unique_ptr<igl::IShaderLibrary> Device::createShaderLibrary(
    const igl::ShaderLibraryDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createShaderLibraryImpl(desc, outResult);
}

inline void Device::updateSurface(void* IGL_NONNULL nativeWindowType) {
  updateSurfaceImpl(nativeWindowType);
}

inline std::unique_ptr<igl::IShaderStages> Device::createShaderStages(
    const igl::ShaderStagesDesc& desc,
    igl::Result* IGL_NULLABLE outResult) const {
  return createShaderStagesImpl(desc, outResult);
}

} // namespace iglu::sentinel
