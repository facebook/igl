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

namespace igl::vulkan {

class Buffer;
class VulkanContext;

/// @brief This class implements the igl::ICommandBuffer interface for Vulkan
class CommandBuffer final : public ICommandBuffer,
                            public std::enable_shared_from_this<CommandBuffer> {
 public:
  /// @brief Constructs a CommandBuffer object, acquires a
  /// `VulkanImmediateCommands::CommandBufferWrapper` from the context's VulkanImmediateCommands
  /// object, and stores the CommandBufferDesc structure used to construct the underlying command
  /// buffer.
  CommandBuffer(VulkanContext& ctx, CommandBufferDesc desc);

  /// @brief Creates a ComputeCommandEncoder
  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  /** @brief Creates a RenderCommandEncoder
   * Before creating a RenderCommandEncoder, this function transitions all images referenced by the
   * `dependencies` parameter to a shader read only layout. It also transitions all images
   * referenced by the `framebuffer` parameter to their optimal layout: color and resolve
   * attachments are transitioned to color attachment optimal layouts; the depth/stencil is
   * transitioned to depth/stencil attachment optimal layout. Once the RenderCommandEncoder has been
   * created, and if there is an enhanced shader debugging store object defined in the contest, this
   * function also binds an extra storage buffer used by the shader debugging functionality. Returns
   * a RenderCommandEncoder object.
   */
  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      const Dependencies& dependencies,
      Result* outResult) override;

  /** @brief Caches the texture passed in to the function for presentation later. Due to the
   * enhanced shader debugging functionality, the image cannot be presented here. It can only be
   * presented after the command buffer has been submitted, processed and then used by the enhanced
   * shader debugging functionality. If the texture belongs to a swapchain, this function
   * transitions the texture to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layout. Otherwise it transitions the
   * texture to the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout if the texture's samples is
   * equal to 1 (it's non multi-sampled).
   */
  void present(std::shared_ptr<ITexture> surface) const override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;

  void popDebugGroupLabel() const override;

  /// @brief Waits until the command bufer has been executed by the device.
  void waitUntilCompleted() override;

  /// @brief Not implemented
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

} // namespace igl::vulkan
