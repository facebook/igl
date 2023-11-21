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
  CommandBuffer(VulkanContext& ctx, CommandBufferDesc desc);

  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      const Dependencies& dependencies,
      Result* outResult) override;

  void present(std::shared_ptr<ITexture> surface) const override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;

  void popDebugGroupLabel() const override;

  void waitUntilCompleted() override;

  void waitUntilScheduled() override;

  VkCommandBuffer getVkCommandBuffer() const {
    return wrapper_.cmdBuf_;
  }

  bool isFromSwapchain() const {
    return isFromSwapchain_;
  }

  std::shared_ptr<igl::IFramebuffer> getFramebuffer() const;

  std::shared_ptr<ITexture> getPresentedSurface() const;

 private:
  friend class CommandQueue;

  VulkanContext& ctx_;
  const VulkanImmediateCommands::CommandBufferWrapper& wrapper_;
  CommandBufferDesc desc_;
  // was present() called with a swapchain image?
  mutable bool isFromSwapchain_ = false;

  std::shared_ptr<igl::IFramebuffer> framebuffer_;
  mutable std::shared_ptr<ITexture> presentedSurface_;

  VulkanImmediateCommands::SubmitHandle lastSubmitHandle_ = {};
};

} // namespace vulkan
} // namespace igl
