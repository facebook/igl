/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "PipelineState.h"

#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanSampler.h>
#include <igl/vulkan/VulkanShaderModule.h>

namespace igl::vulkan {

void PipelineState::initializeSpvModuleInfoFromShaderStages(const VulkanContext& ctx,
                                                            IShaderStages* stages) {
  auto* smComp = static_cast<igl::vulkan::ShaderModule*>(stages->getComputeModule().get());

  VkShaderStageFlags pushConstantMask = 0;

  if (smComp) {
    // compute
    ensureShaderModule(smComp);

    info_ = smComp->getVulkanShaderModule().getSpvModuleInfo();

    if (info_.hasPushConstants) {
      pushConstantMask |= VK_SHADER_STAGE_COMPUTE_BIT;
    }

    stageFlags_ = VK_SHADER_STAGE_COMPUTE_BIT;
  } else {
    auto* smVert = static_cast<igl::vulkan::ShaderModule*>(stages->getVertexModule().get());
    auto* smFrag = static_cast<igl::vulkan::ShaderModule*>(stages->getFragmentModule().get());

    // vertex/fragment
    ensureShaderModule(smVert);
    ensureShaderModule(smFrag);

    const util::SpvModuleInfo& infoVert = smVert->getVulkanShaderModule().getSpvModuleInfo();
    const util::SpvModuleInfo& infoFrag = smFrag->getVulkanShaderModule().getSpvModuleInfo();

    if (infoVert.hasPushConstants) {
      pushConstantMask |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (infoFrag.hasPushConstants) {
      pushConstantMask |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    info_ = util::mergeReflectionData(infoVert, infoFrag);

    stageFlags_ = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  if (pushConstantMask) {
    const VkPhysicalDeviceLimits& limits = ctx.getVkPhysicalDeviceProperties().limits;

    constexpr uint32_t kPushConstantsSize = 128;

    if (!IGL_VERIFY(kPushConstantsSize <= limits.maxPushConstantsSize)) {
      IGL_LOG_ERROR("Push constants size exceeded %u (max %u bytes)",
                    kPushConstantsSize,
                    limits.maxPushConstantsSize);
    }

    pushConstantRange_ = ivkGetPushConstantRange(pushConstantMask, 0, kPushConstantsSize);
  }
}

PipelineState::PipelineState(
    const VulkanContext& ctx,
    IShaderStages* stages,
    std::shared_ptr<ISamplerState> immutableSamplers[IGL_TEXTURE_SAMPLERS_MAX],
    const char* debugName) {
  IGL_ASSERT(stages);

  initializeSpvModuleInfoFromShaderStages(ctx, stages);

  // Create all Vulkan descriptor set layouts for this pipeline

  // 0. Combined image samplers
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info_.textures.size());
    for (const auto& t : info_.textures) {
      const uint32_t loc = t.bindingLocation;
      bindings.emplace_back(ivkGetDescriptorSetLayoutBinding(
          loc, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stageFlags_));
      if (loc < IGL_TEXTURE_SAMPLERS_MAX && immutableSamplers && immutableSamplers[loc]) {
        auto* sampler = static_cast<igl::vulkan::SamplerState*>(immutableSamplers[loc].get());
        bindings.back().pImmutableSamplers = &sampler->sampler_->vkSampler_;
      }
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslCombinedImageSamplers_ = std::make_unique<VulkanDescriptorSetLayout>(
        ctx,
        VkDescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data(),
        bindingFlags.data(),
        IGL_FORMAT("Descriptor Set Layout (COMBINED_IMAGE_SAMPLER): {}", debugName).c_str());
  }
  // 1. Uniform buffers
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info_.uniformBuffers.size());
    for (const auto& b : info_.uniformBuffers) {
      bindings.emplace_back(ivkGetDescriptorSetLayoutBinding(
          b.bindingLocation, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, stageFlags_));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslUniformBuffers_ = std::make_unique<VulkanDescriptorSetLayout>(
        ctx,
        VkDescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data(),
        bindingFlags.data(),
        IGL_FORMAT("Descriptor Set Layout (UNIFORM_BUFFER): {}", debugName).c_str());
  }
  // 2. Storage buffers
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info_.storageBuffers.size());
    for (const auto& b : info_.storageBuffers) {
      bindings.emplace_back(ivkGetDescriptorSetLayoutBinding(
          b.bindingLocation, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stageFlags_));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslStorageBuffers_ = std::make_unique<VulkanDescriptorSetLayout>(
        ctx,
        VkDescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data(),
        bindingFlags.data(),
        IGL_FORMAT("Descriptor Set Layout (STORAGE_BUFFER): {}", debugName).c_str());
  }
}

VkPipelineLayout PipelineState::getVkPipelineLayout() const {
  IGL_ASSERT(pipelineLayout_);

  return pipelineLayout_->getVkPipelineLayout();
}

} // namespace igl::vulkan
