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

namespace igl {
namespace vulkan {

class VulkanContext;
class VulkanShaderModule;
struct DeviceQueues;

class Device final : public IDevice {
 public:
  explicit Device(std::unique_ptr<VulkanContext> ctx);

  // Command Queue
  std::shared_ptr<ICommandQueue> createCommandQueue(CommandQueueType type,
                                                    Result* outResult) override;
  // Resources
  std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc,
                                        Result* outResult) const noexcept override;

  std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                    Result* outResult) const override;
  std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                          Result* outResult) const noexcept override;

  // Pipelines
  std::shared_ptr<IComputePipelineState> createComputePipeline(const ComputePipelineDesc& desc,
                                                               Result* outResult) const override;
  std::shared_ptr<IRenderPipelineState> createRenderPipeline(const RenderPipelineDesc& desc,
                                                             Result* outResult) const override;

  // Shaders
  std::shared_ptr<IShaderModule> createShaderModule(const ShaderModuleDesc& desc,
                                                    Result* outResult) const override;

  std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                  Result* outResult) override;

  // Platform-specific extensions
  const PlatformDevice& getPlatformDevice() const noexcept override;

  VulkanContext& getVulkanContext() {
    return *ctx_.get();
  }
  const VulkanContext& getVulkanContext() const {
    return *ctx_.get();
  }

 private:
  std::shared_ptr<VulkanShaderModule> createShaderModule(const void* data,
                                                         size_t length,
                                                         const std::string& debugName,
                                                         Result* outResult) const;
  std::shared_ptr<VulkanShaderModule> createShaderModule(ShaderStage stage,
                                                         const char* source,
                                                         const std::string& debugName,
                                                         Result* outResult) const;

  std::unique_ptr<VulkanContext> ctx_;

  PlatformDevice platformDevice_;
};

} // namespace vulkan
} // namespace igl
