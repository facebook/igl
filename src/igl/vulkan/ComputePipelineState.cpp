/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/ComputePipelineState.h>

#include <utility>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>

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
        [vf = &ctx.vf_, device = device_.getVulkanContext().getVkDevice(), pipeline = pipeline_]() {
          vf->vkDestroyPipeline(device, pipeline, nullptr);
        }));
  }
  if (pipelineLayout != VK_NULL_HANDLE) {
    const auto& ctx = device_.getVulkanContext();
    ctx.deferredTask(std::packaged_task<void()>(
        [vf = &ctx.vf_, device = ctx.getVkDevice(), layout = pipelineLayout]() {
          vf->vkDestroyPipelineLayout(device, layout, nullptr);
        }));
  }
}

VkPipeline ComputePipelineState::getVkPipeline() const {
  const VulkanContext& ctx = device_.getVulkanContext();

  if (ctx.config_.enableDescriptorIndexing) {
    // the bindless descriptor set layout can be changed in VulkanContext when the number of
    // existing textures increases
    if (lastBindlessVkDescriptorSetLayout != ctx.getBindlessVkDescriptorSetLayout()) {
      // there's a new descriptor set layout - drop the previous Vulkan pipeline
      VkDevice device = ctx.getVkDevice();
      if (pipeline_ != VK_NULL_HANDLE) {
        ctx.deferredTask(std::packaged_task<void()>(
            [vf = &ctx.vf_, device, pipeline = pipeline_, layout = pipelineLayout]() {
              vf->vkDestroyPipeline(device, pipeline, nullptr);
              vf->vkDestroyPipelineLayout(device, layout, nullptr);
            }));
      }
      pipeline_ = VK_NULL_HANDLE;
      pipelineLayout = VK_NULL_HANDLE;
      lastBindlessVkDescriptorSetLayout = ctx.getBindlessVkDescriptorSetLayout();
    }
  }

  if (pipeline_ != VK_NULL_HANDLE) {
    return pipeline_;
  }

  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  // NOLINTBEGIN(readability-identifier-naming)
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  const VkDescriptorSetLayout DSLs[] = {
      dslCombinedImageSamplers->getVkDescriptorSetLayout(),
      dslBuffers->getVkDescriptorSetLayout(),
      dslStorageImages->getVkDescriptorSetLayout(),
      ctx.getBindlessVkDescriptorSetLayout(),
  };
  // NOLINTEND(readability-identifier-naming)

  const VkPipelineLayoutCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(ctx.config_.enableDescriptorIndexing
                                                  ? IGL_ARRAY_NUM_ELEMENTS(DSLs)
                                                  : IGL_ARRAY_NUM_ELEMENTS(DSLs) - 1u),
      .pSetLayouts = DSLs,
      .pushConstantRangeCount = info.hasPushConstants ? 1u : 0u,
      .pPushConstantRanges = info.hasPushConstants ? &pushConstantRange : nullptr,
  };

  VkDevice device = ctx.getVkDevice();
  VK_ASSERT(ctx.vf_.vkCreatePipelineLayout(device, &ci, nullptr, &pipelineLayout));
  VK_ASSERT(
      ivkSetDebugObjectName(&ctx.vf_,
                            device,
                            VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                            (uint64_t)pipelineLayout,
                            IGL_FORMAT("Pipeline Layout: {}", desc_.debugName.c_str()).c_str()));

  const auto& shaderModule = desc_.shaderStages->getComputeModule();

  VulkanComputePipelineBuilder()
      .shaderStage(VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_COMPUTE_BIT,
          .module = igl::vulkan::ShaderModule::getVkShaderModule(shaderModule),
          .pName = shaderModule->info().entryPoint.c_str(),
      })
      .build(
          ctx.vf_, device, ctx.pipelineCache_, pipelineLayout, &pipeline_, desc_.debugName.c_str());

  return pipeline_;
}

} // namespace igl::vulkan
