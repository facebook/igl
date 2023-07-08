/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/ResourcesBinder.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanPipelineLayout.h>

namespace igl {

namespace vulkan {

ResourcesBinder::ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                 const VulkanContext& ctx,
                                 VkPipelineBindPoint bindPoint) :
  ctx_(ctx), cmdBuffer_(commandBuffer->getVkCommandBuffer()), bindPoint_(bindPoint) {}

void ResourcesBinder::bindPipeline(VkPipeline pipeline) {
  if (lastPipelineBound_ == pipeline) {
    return;
  }

  lastPipelineBound_ = pipeline;

  if (pipeline != VK_NULL_HANDLE) {
#if IGL_VULKAN_PRINT_COMMANDS
    LLOGL("%p vkCmdBindPipeline(%u, %p)\n", cmdBuffer_, bindPoint_, pipeline);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vkCmdBindPipeline(cmdBuffer_, bindPoint_, pipeline);
  }
}

} // namespace vulkan
} // namespace igl
