/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <utility>

namespace igl {
namespace vulkan {

ComputePipelineState::ComputePipelineState(const igl::vulkan::Device& device,
                                           // Ignore modernize-pass-by-value
                                           // @lint-ignore CLANGTIDY
                                           const ComputePipelineDesc& desc) :
  device_(device),
  // Ignore modernize-pass-by-value
  // @lint-ignore CLANGTIDY
  desc_(desc) {
  vkPipelineLayout_ = device_.getVulkanContext().pipelineLayoutCompute_->getVkPipelineLayout();
}

ComputePipelineState ::~ComputePipelineState() {
  if (pipeline_ != VK_NULL_HANDLE) {
    const auto& ctx = device_.getVulkanContext();
    ctx.deferredTask(std::packaged_task<void()>(
        [vf = &ctx.vf_,
         device = device_.getVulkanContext().device_->getVkDevice(),
         pipeline = pipeline_]() { vf->vkDestroyPipeline(device, pipeline, nullptr); }));
  }
}

VkPipeline ComputePipelineState::getVkPipeline() const {
  const VulkanContext& ctx = device_.getVulkanContext();
  if (vkPipelineLayout_ != ctx.pipelineLayoutCompute_->getVkPipelineLayout()) {
    // there's a new pipeline layout - drop the previous Vulkan pipeline
    VkDevice device = ctx.device_->getVkDevice();
    if (pipeline_ != VK_NULL_HANDLE) {
      ctx.deferredTask(std::packaged_task<void()>([vf = &ctx.vf_, device, pipeline = pipeline_]() {
        vf->vkDestroyPipeline(device, pipeline, nullptr);
      }));
    }
    pipeline_ = VK_NULL_HANDLE;
    vkPipelineLayout_ = ctx.pipelineLayoutCompute_->getVkPipelineLayout();
  }

  if (pipeline_ != VK_NULL_HANDLE) {
    return pipeline_;
  }

  const auto& shaderModule = desc_.shaderStages->getComputeModule();

  igl::vulkan::VulkanComputePipelineBuilder()
      .shaderStage(ivkGetPipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_COMPUTE_BIT,
          igl::vulkan::ShaderModule::getVkShaderModule(shaderModule),
          shaderModule->info().entryPoint.c_str()))
      .build(ctx.vf_,
             ctx.device_->getVkDevice(),
             ctx.pipelineCache_,
             ctx.pipelineLayoutCompute_->getVkPipelineLayout(),
             &pipeline_,
             desc_.debugName.c_str());

  return pipeline_;
}

} // namespace vulkan
} // namespace igl
