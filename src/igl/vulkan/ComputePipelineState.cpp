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
#include <igl/vulkan/VulkanShaderModule.h>
#include <utility>

namespace lvk::vulkan {

ComputePipelineState::ComputePipelineState(lvk::vulkan::Device* device,
                                           const ComputePipelineDesc& desc) :
  device_(device), desc_(desc) {}

ComputePipelineState::ComputePipelineState(ComputePipelineState&& other) :
  device_(other.device_), desc_(std::move(other.desc_)), pipeline_(other.pipeline_) {
  other.device_ = nullptr;
  other.pipeline_ = VK_NULL_HANDLE;
}

ComputePipelineState& ComputePipelineState::operator=(ComputePipelineState&& other) {
  std::swap(device_, other.device_);
  std::swap(desc_, other.desc_);
  std::swap(pipeline_, other.pipeline_);
  return *this;
}

ComputePipelineState ::~ComputePipelineState() {
  if (!device_) {
    return;
  }

  device_->destroy(desc_.shaderStages.getModule(Stage_Compute));

  if (pipeline_ != VK_NULL_HANDLE) {
    device_->getVulkanContext().deferredTask(std::packaged_task<void()>(
        [device = device_->getVulkanContext().getVkDevice(), pipeline = pipeline_]() {
          vkDestroyPipeline(device, pipeline, nullptr);
        }));
  }
}

VkPipeline ComputePipelineState::getVkPipeline() const {
  assert(device_);

  if (!device_) {
    return VK_NULL_HANDLE;
  }

  if (pipeline_ != VK_NULL_HANDLE) {
    return pipeline_;
  }

  const VulkanContext& ctx = device_->getVulkanContext();

  const VulkanShaderModule* sm =
      ctx.shaderModulesPool_.get(desc_.shaderStages.getModule(Stage_Compute));

  VkShaderModule vkShaderModule = sm ? sm->getVkShaderModule() : VK_NULL_HANDLE;

  const VkComputePipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stage = ivkGetPipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_COMPUTE_BIT, vkShaderModule, sm->getEntryPoint()),
      .layout = ctx.vkPipelineLayout_,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  VK_ASSERT(
      vkCreateComputePipelines(ctx.getVkDevice(), ctx.pipelineCache_, 1, &ci, nullptr, &pipeline_));

  VK_ASSERT(ivkSetDebugObjectName(
      ctx.getVkDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline_, desc_.debugName));

  return pipeline_;
}

} // namespace lvk::vulkan
