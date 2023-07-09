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
  desc_(desc) {}

ComputePipelineState ::~ComputePipelineState() {
  if (pipeline_ != VK_NULL_HANDLE) {
    device_.getVulkanContext().deferredTask(std::packaged_task<void()>(
        [device = device_.getVulkanContext().device_->getVkDevice(), pipeline = pipeline_]() {
          vkDestroyPipeline(device, pipeline, nullptr);
        }));
  }
}

VkPipeline ComputePipelineState::getVkPipeline() const {
  if (pipeline_ != VK_NULL_HANDLE) {
    return pipeline_;
  }

  const VulkanContext& ctx = device_.getVulkanContext();

  const ShaderModule* shaderModule =
      static_cast<const ShaderModule*>(desc_.shaderStages->getModule(Stage_Compute));

  igl::vulkan::VulkanComputePipelineBuilder()
      .shaderStage(ivkGetPipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_COMPUTE_BIT,
          igl::vulkan::ShaderModule::getVkShaderModule(shaderModule),
          shaderModule->desc().entryPoint))
      .build(
          ctx.device_->getVkDevice(),
          // TODO: use ctx.pipelineCache_
          VK_NULL_HANDLE,
          ctx.pipelineLayout_->getVkPipelineLayout(),
          &pipeline_,
          desc_.debugName);

  return pipeline_;
}

} // namespace vulkan
} // namespace igl
