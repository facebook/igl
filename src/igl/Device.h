/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/Texture.h>
#include <utility>

namespace igl {

class IBuffer;
class ICommandBuffer;
class IFramebuffer;
class IRenderPipelineState;
class IShaderModule;
struct BufferDesc;
struct Framebuffer;
struct RenderPipelineDesc;
struct ShaderModuleDesc;
struct ShaderStages;

struct ComputePipelineDesc {
  std::shared_ptr<ShaderStages> shaderStages;
  std::string debugName;
};

class IComputePipelineState {
 public:
  virtual ~IComputePipelineState() = default;
};

/**
 * @brief Interface to a GPU that is used to draw graphics or do parallel computation.
 */
class IDevice {
 public:
  virtual ~IDevice() = default;

  virtual std::shared_ptr<ICommandBuffer> createCommandBuffer() = 0;

  virtual void submit(igl::CommandQueueType queueType,
                      const ICommandBuffer& commandBuffer,
                      bool present = false) const = 0;

  virtual std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                                Result* outResult) const noexcept = 0;

  virtual std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                            Result* outResult) const = 0;

  virtual std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                                  Result* outResult) const noexcept = 0;

  virtual std::shared_ptr<IComputePipelineState> createComputePipeline(
      const ComputePipelineDesc& desc,
      Result* outResult) const = 0;

  virtual std::shared_ptr<IRenderPipelineState> createRenderPipeline(const RenderPipelineDesc& desc,
                                                                     Result* outResult) const = 0;

  virtual std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                            Result* outResult) const = 0;

  virtual std::shared_ptr<ITexture> getCurrentSwapchainTexture() = 0;

  virtual std::unique_ptr<ShaderStages> createShaderStages(const char* vs,
                                                           std::string debugNameVS,
                                                           const char* fs,
                                                           std::string debugNameFS,
                                                           Result* outResult = nullptr) const;
  virtual std::unique_ptr<ShaderStages> createShaderStages(const char* cs,
                                                           std::string debugName,
                                                           Result* outResult = nullptr) const;

 protected:
  IDevice() = default;
};

} // namespace igl
