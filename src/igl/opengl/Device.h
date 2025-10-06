/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Device.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/UnbindPolicy.h>

namespace igl::opengl {
class CommandQueue;

class Device : public IDevice {
  friend class HWDevice;
  friend class PlatformDevice;

 public:
  explicit Device(std::unique_ptr<IContext> context);
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

  // Backend type query
  [[nodiscard]] BackendType getBackendType() const override {
    return BackendType::OpenGL;
  }

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

  std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                  Result* IGL_NULLABLE outResult) noexcept override;

  // Timers
  std::shared_ptr<ITimer> createTimer(Result* IGL_NULLABLE outResult) const noexcept override;

  // debug markers useful in GPU captures
  void pushMarker(int len, const char* IGL_NULLABLE name);
  void popMarker();

  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override = 0;

  void setCurrentThread() override;

  [[nodiscard]] IContext& getContext() const {
    return *context_;
  }

  // ICapabilities
  [[nodiscard]] bool hasFeature(DeviceFeatures capability) const override;
  [[nodiscard]] bool hasRequirement(DeviceRequirement requirement) const override;
  bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const override;
  [[nodiscard]] TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat format) const override;
  [[nodiscard]] ShaderVersion getShaderVersion() const override;
  [[nodiscard]] BackendVersion getBackendVersion() const override;

  // Device Statistics
  [[nodiscard]] size_t getCurrentDrawCount() const override;
  [[nodiscard]] size_t getShaderCompilationCount() const override;

  bool verifyScope() override;

  void updateSurface(void* IGL_NULLABLE nativeWindowType) override;

 protected:
  void beginScope() override;
  void endScope() override;

  [[nodiscard]] const std::shared_ptr<IContext>& getSharedContext() const {
    return context_;
  }

 private:
  GLint defaultFrameBufferID_{};
  GLint defaultFrameBufferWidth_{};
  GLint defaultFrameBufferHeight_{};
  const std::shared_ptr<IContext> context_;
  // on OpenGL we only need one command queue
  std::shared_ptr<CommandQueue> commandQueue_;
  const DeviceFeatureSet& deviceFeatureSet_;
  UnbindPolicy cachedUnbindPolicy_;
};

} // namespace igl::opengl
