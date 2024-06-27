/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <igl/Device.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/UnbindPolicy.h>

namespace igl::opengl {
class CommandQueue;
class Texture;

class Device : public IDevice {
  friend class HWDevice;
  friend class PlatformDevice;

 public:
  Device(std::unique_ptr<IContext> context);
  ~Device() override;

  // Command Queue
  std::shared_ptr<ICommandQueue> createCommandQueue(const CommandQueueDesc& desc,
                                                    Result* outResult) override;

  // Backend type query
  [[nodiscard]] BackendType getBackendType() const override {
    return BackendType::OpenGL;
  }

  // Resources
  std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                        Result* outResult) const noexcept override;
  std::shared_ptr<IDepthStencilState> createDepthStencilState(const DepthStencilStateDesc& desc,
                                                              Result* outResult) const override;
  std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                    Result* outResult) const override;
  std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                          Result* outResult) const noexcept override;

  std::shared_ptr<IVertexInputState> createVertexInputState(const VertexInputStateDesc& desc,
                                                            Result* outResult) const override;

  // Pipelines
  std::shared_ptr<IComputePipelineState> createComputePipeline(const ComputePipelineDesc& desc,
                                                               Result* outResult) const override;
  std::shared_ptr<IRenderPipelineState> createRenderPipeline(const RenderPipelineDesc& desc,
                                                             Result* outResult) const override;

  // Shaders
  std::unique_ptr<IShaderLibrary> createShaderLibrary(const ShaderLibraryDesc& desc,
                                                      Result* outResult) const override;

  std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                    Result* outResult) const override;

  std::unique_ptr<IShaderStages> createShaderStages(const ShaderStagesDesc& desc,
                                                    Result* outResult) const override;

  std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                  Result* outResult) override;

  // debug markers useful in GPU captures
  void pushMarker(int len, const char* name);
  void popMarker();

  [[nodiscard]] const PlatformDevice& getPlatformDevice() const noexcept override = 0;

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

  // Device Statistics
  [[nodiscard]] size_t getCurrentDrawCount() const override;

  bool verifyScope() override;

  void updateSurface(void* nativeWindowType) override;

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
