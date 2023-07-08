/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/Texture.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl {
namespace vulkan {

class Buffer;
class SamplerState;
class Texture;

class ResourcesBinder final {
 public:
  ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                  const VulkanContext& ctx,
                  VkPipelineBindPoint bindPoint);

  void bindPipeline(VkPipeline pipeline);

 private:
  friend class VulkanContext;

  bool isGraphics() const {
    return bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS;
  }

 private:
  const VulkanContext& ctx_;
  VkCommandBuffer cmdBuffer_ = VK_NULL_HANDLE;
  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;
  VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace vulkan
} // namespace igl
