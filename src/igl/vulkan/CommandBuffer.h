/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl {
namespace vulkan {

class Buffer;
class VulkanContext;

class CommandBuffer final : public ICommandBuffer,
                            public std::enable_shared_from_this<CommandBuffer> {
 public:
  explicit CommandBuffer(VulkanContext& ctx);

  virtual std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      Result* outResult) override;

  void present(std::shared_ptr<ITexture> surface) const override;

  void waitUntilCompleted() override;

  void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  void dispatchThreadGroups(const Dimensions& threadgroupCount) override;

  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;
  void insertDebugEventLabel(const std::string& label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;
  void useComputeTexture(const std::shared_ptr<ITexture>& texture) override;
  void bindPushConstants(size_t offset, const void* data, size_t length) override;

  VkCommandBuffer getVkCommandBuffer() const {
    return wrapper_.cmdBuf_;
  }

  bool isFromSwapchain() const {
    return isFromSwapchain_;
  }

 private:
  friend class Device;

  VulkanContext& ctx_;
  const VulkanImmediateCommands::CommandBufferWrapper& wrapper_;
  // was present() called with a swapchain image?
  mutable bool isFromSwapchain_ = false;

  std::shared_ptr<igl::IFramebuffer> framebuffer_;

  VulkanImmediateCommands::SubmitHandle lastSubmitHandle_ = {};

  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
