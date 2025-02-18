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
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <utility>

namespace igl::vulkan {

ComputePipelineState::ComputePipelineState(const igl::vulkan::Device& device,
                                           ComputePipelineDesc desc) :
  PipelineState(device.getVulkanContext(),
                desc.shaderStages.get(),
                nullptr,
                0,
                desc.debugName.c_str()),
  device_(device),
  desc_(std::move(desc)) {}

ComputePipelineState ::~ComputePipelineState() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (pipeline_ != VK_NULL_HANDLE) {
    const auto& ctx = device_.getVulkanContext();
    ctx.deferredTask(std::packaged_task<void()>(
        [vf = &ctx.vf_,
         device = device_.getVulkanContext().device_->getVkDevice(),
         pipeline = pipeline_]() { vf->vkDestroyPipeline(device, pipeline, nullptr); }));
  }
  if (pipelineLayout_ != VK_NULL_HANDLE) {
    const auto& ctx = device_.getVulkanContext();
    ctx.deferredTask(std::packaged_task<void()>(
        [vf = &ctx.vf_, device = ctx.getVkDevice(), layout = pipelineLayout_]() {
          vf->vkDestroyPipelineLayout(device, layout, nullptr);
        }));
  }
}

VkPipeline ComputePipelineState::getVkPipeline() const {
  const VulkanContext& ctx = device_.getVulkanContext();

  if (ctx.config_.enableDescriptorIndexing) {
    // the bindless descriptor set layout can be changed in VulkanContext when the number of
    // existing textures increases
    if (lastBindlessVkDescriptorSetLayout_ != ctx.getBindlessVkDescriptorSetLayout()) {
      // there's a new descriptor set layout - drop the previous Vulkan pipeline
      VkDevice device = ctx.device_->getVkDevice();
      if (pipeline_ != VK_NULL_HANDLE) {
        ctx.deferredTask(std::packaged_task<void()>(
            [vf = &ctx.vf_, device, pipeline = pipeline_, layout = pipelineLayout_]() {
              vf->vkDestroyPipeline(device, pipeline, nullptr);
              vf->vkDestroyPipelineLayout(device, layout, nullptr);
            }));
      }
      pipeline_ = VK_NULL_HANDLE;
      pipelineLayout_ = VK_NULL_HANDLE;
      lastBindlessVkDescriptorSetLayout_ = ctx.getBindlessVkDescriptorSetLayout();
    }
  }

  if (pipeline_ != VK_NULL_HANDLE) {
    return pipeline_;
  }

  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  // @fb-only
  const VkDescriptorSetLayout DSLs[] = {
      dslCombinedImageSamplers_->getVkDescriptorSetLayout(),
      dslBuffers_->getVkDescriptorSetLayout(),
      dslStorageImages_->getVkDescriptorSetLayout(),
      ctx.getBindlessVkDescriptorSetLayout(),
  };

  const VkPipelineLayoutCreateInfo ci =
      ivkGetPipelineLayoutCreateInfo(static_cast<uint32_t>(ctx.config_.enableDescriptorIndexing
                                                               ? IGL_ARRAY_NUM_ELEMENTS(DSLs)
                                                               : IGL_ARRAY_NUM_ELEMENTS(DSLs) - 1u),
                                     DSLs,
                                     info_.hasPushConstants ? &pushConstantRange_ : nullptr);

  VkDevice device = ctx.device_->getVkDevice();
  VK_ASSERT(ctx.vf_.vkCreatePipelineLayout(device, &ci, nullptr, &pipelineLayout_));
  VK_ASSERT(
      ivkSetDebugObjectName(&ctx.vf_,
                            device,
                            VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                            (uint64_t)pipelineLayout_,
                            IGL_FORMAT("Pipeline Layout: {}", desc_.debugName.c_str()).c_str()));

  const auto& shaderModule = desc_.shaderStages->getComputeModule();

  VulkanComputePipelineBuilder()
      .shaderStage(ivkGetPipelineShaderStageCreateInfo(
          VK_SHADER_STAGE_COMPUTE_BIT,
          igl::vulkan::ShaderModule::getVkShaderModule(shaderModule),
          shaderModule->info().entryPoint.c_str()))
      .build(ctx.vf_,
             ctx.device_->getVkDevice(),
             ctx.pipelineCache_,
             pipelineLayout_,
             &pipeline_,
             desc_.debugName.c_str());

  return pipeline_;
}

} // namespace igl::vulkan
