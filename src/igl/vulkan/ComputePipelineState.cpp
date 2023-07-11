/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanShaderModule.h>
#include <utility>

namespace igl {
namespace vulkan {

ComputePipelineState::ComputePipelineState(const igl::vulkan::Device& device,
                                           const ComputePipelineDesc& desc) :
  device_(device),
  desc_(desc) {}

ComputePipelineState ::~ComputePipelineState() {
  if (pipeline_ != VK_NULL_HANDLE) {
    device_.getVulkanContext().deferredTask(std::packaged_task<void()>(
        [device = device_.getVulkanContext().getVkDevice(), pipeline = pipeline_]() {
          vkDestroyPipeline(device, pipeline, nullptr);
        }));
  }
}

VkPipeline ComputePipelineState::getVkPipeline() const {
  if (pipeline_ != VK_NULL_HANDLE) {
    return pipeline_;
  }

  const VulkanContext& ctx = device_.getVulkanContext();

  const VulkanShaderModule* sm = device_.getShaderModule(desc_.computeShaderModule);

  VkShaderModule vkShaderModule = sm ? sm->getVkShaderModule() : VK_NULL_HANDLE;

  const VkComputePipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stage = ivkGetPipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_COMPUTE_BIT, vkShaderModule, sm->getEntryPoint()),
      .layout = ctx.pipelineLayout_->getVkPipelineLayout(),
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  // TODO: use ctx.pipelineCache_
  VK_ASSERT(
      vkCreateComputePipelines(ctx.getVkDevice(), VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline_));

  VK_ASSERT(ivkSetDebugObjectName(
      ctx.getVkDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline_, desc_.debugName));

  return pipeline_;
}

} // namespace vulkan
} // namespace igl
