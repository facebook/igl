/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandQueue.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>

namespace igl::vulkan {

class CommandBuffer;

/** @brief Implements the igl::ICommandQueue interface for Vulkan. Currently, this class only
 * supports one command buffer active at a time, tracked by an internal flag set to true in
 * `createCommandBuffer()` and reset in `endCommandBuffer()` (automatically called from `submit()`).
 * This class also implements shader debugging facilities, which are executed after a command buffer
 * is submitted. This extra pass is only executed if the context provides additional information for
 * rendering. It is disabled by default.
 */
class CommandQueue final : public ICommandQueue {
 public:
  CommandQueue(Device& device, const CommandQueueDesc& desc);

  ~CommandQueue() override = default;

  /// @brief Create a new command buffer. Sets the internal flag that tracks an active command
  /// buffer has been created. If we cannot create a command buffer, this function will return
  /// null.
  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* outResult) override;

  /// @brief Submits the `commandBuffer` for execution on the GPU. If the enhanced shader debugging
  /// is enabled (stored data is available in the context), this function will install barriers
  /// before the command buffer is executed. It will also execute the shader debugging render pass
  /// by calling `enhancedShaderDebuggingPass()`. If the enhanced shader debugging is enabled,
  /// presenting the image is disabled.
  /// @param cmdBuffer The command buffer to be submitted.
  /// @param endOfFrame Not used
  SubmitHandle submit(const ICommandBuffer& cmdBuffer, bool endOfFrame = false) override;

  [[nodiscard]] const CommandQueueDesc& getCommandQueueDesc() const {
    return desc_;
  }

 private:
  /** @brief Ends the current command buffer and resets the internal flag tracking an active command
   * buffer. Determines if an image should be presented by (1) checking if this instance belongs to
   * a graphics queue, (2) the context has a swapchain object, (3) the command buffer is from a
   * swapchain (please refer to CommandBuffer::present), and (4) the present parameter is true. If
   * so, this function waits for the swapchain semaphore before submitting the command buffer for
   * execution. After the command buffer is submitted, this function calls VulkanContext::present()
   * if an image should be presented. Finally, it signals the context to process deferred tasks (for
   * more details about deferred tasks, please refer to the igl::vulkan::VulkanContext class).
   */
  SubmitHandle endCommandBuffer(const igl::vulkan::VulkanContext& ctx,
                                igl::vulkan::CommandBuffer* cmdBuffer,
                                bool present);

  /// @brief Executes the shader debugging render pass. Also presents the image if the command
  /// buffer being submitted was from a swapchain.
  void enhancedShaderDebuggingPass(const igl::vulkan::VulkanContext& ctx,
                                   const igl::vulkan::CommandBuffer* cmdBuffer);

 private:
  igl::vulkan::Device& device_;
  CommandQueueDesc desc_;

  /// @brief Flag indicating whether or not there is an active command buffer. Currently only one
  /// command buffer can be active at a time.
  bool isInsideFrame_ = false;
};

} // namespace igl::vulkan
