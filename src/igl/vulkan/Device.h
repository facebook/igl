/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/LVK.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanSemaphore.h>
#include <memory>
#include <vector>

namespace igl {
namespace vulkan {

class VulkanContext;
class VulkanContextConfig;
class VulkanShaderModule;

class Device final : public IDevice {
 public:
  explicit Device(std::unique_ptr<VulkanContext> ctx);

  std::shared_ptr<ICommandBuffer> createCommandBuffer() override;

  void submit(const igl::ICommandBuffer& commandBuffer,
              igl::CommandQueueType queueTyp,
              ITexture* present) override;

  // Resources
  std::unique_ptr<IBuffer> createBuffer(const BufferDesc& desc, Result* outResult) override;

  std::shared_ptr<ISamplerState> createSamplerState(const SamplerStateDesc& desc,
                                                    Result* outResult) override;
  std::shared_ptr<ITexture> createTexture(const TextureDesc& desc,
                                          const char* debugName,
                                          Result* outResult) override;

  // Pipelines
  std::shared_ptr<IComputePipelineState> createComputePipeline(const ComputePipelineDesc& desc,
                                                               Result* outResult) override;
  std::shared_ptr<IRenderPipelineState> createRenderPipeline(const RenderPipelineDesc& desc,
                                                             Result* outResult) override;

  // Shaders
  ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc, Result* outResult) override;

  std::shared_ptr<ITexture> getCurrentSwapchainTexture() override;

  VulkanContext& getVulkanContext() {
    return *ctx_.get();
  }
  const VulkanContext& getVulkanContext() const {
    return *ctx_.get();
  }

  static std::unique_ptr<VulkanContext> createContext(const VulkanContextConfig& config,
                                                      void* window,
                                                      void* display = nullptr);

  static std::vector<HWDeviceDesc> queryDevices(VulkanContext& ctx,
                                                HWDeviceType deviceType,
                                                Result* outResult = nullptr);

  static std::unique_ptr<IDevice> create(std::unique_ptr<VulkanContext> ctx,
                                         const HWDeviceDesc& desc,
                                         uint32_t width,
                                         uint32_t height,
                                         Result* outResult = nullptr);

 private:
  friend class ComputePipelineState;
  friend class RenderPipelineState;

  VulkanShaderModule* getShaderModule(ShaderModuleHandle handle) const;
  std::shared_ptr<VulkanShaderModule> createShaderModule(const void* data,
                                                         size_t length,
                                                         const char* entryPoint,
                                                         const char* debugName,
                                                         Result* outResult) const;
  std::shared_ptr<VulkanShaderModule> createShaderModule(ShaderStage stage,
                                                         const char* source,
                                                         const char* entryPoint,
                                                         const char* debugName,
                                                         Result* outResult) const;

  std::unique_ptr<VulkanContext> ctx_;

  std::vector<std::shared_ptr<VulkanShaderModule>> shaderModules_ = { nullptr };
};

} // namespace vulkan
} // namespace igl
