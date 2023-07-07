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

namespace igl {
namespace vulkan {

class CommandBuffer;

class CommandQueue final : public ICommandQueue {
 public:
  CommandQueue(Device& device, const CommandQueueDesc& desc);

  ~CommandQueue() override = default;

  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* outResult) override;
  SubmitHandle submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

  const CommandQueueDesc& getCommandQueueDesc() const {
    return desc_;
  }

 private:
  SubmitHandle endCommandBuffer(const igl::vulkan::VulkanContext& ctx,
                                igl::vulkan::CommandBuffer* cmdBuffer,
                                bool present);

  void enhancedShaderDebuggingPass(const igl::vulkan::VulkanContext& ctx,
                                   const igl::vulkan::CommandBuffer* cmdBuffer);

 private:
  igl::vulkan::Device& device_;
  CommandQueueDesc desc_;
  bool isInsideFrame_ = false;
};

} // namespace vulkan
} // namespace igl
