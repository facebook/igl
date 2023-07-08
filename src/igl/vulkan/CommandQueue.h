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
  CommandQueue(Device& device, CommandQueueType type);

  ~CommandQueue() override = default;

  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* outResult) override;
  void submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

 private:
  void endCommandBuffer(const igl::vulkan::VulkanContext& ctx,
                        igl::vulkan::CommandBuffer* cmdBuffer,
                        bool present);
 private:
  igl::vulkan::Device& device_;
  CommandQueueType queueType_;
  bool isInsideFrame_ = false;
};

} // namespace vulkan
} // namespace igl
