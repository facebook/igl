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
#include <igl/vulkan/VulkanShaderModule.h>

namespace igl::vulkan {

void PipelineState::initializeSpvModuleInfoFromShaderStages(const VulkanContext& ctx,
                                                            IShaderStages* stages) {
  const ShaderStagesType shaderStagesType = stages->getType();

  VkShaderStageFlags pushConstantMask = 0;

  switch (shaderStagesType) {
  case igl::ShaderStagesType::Compute: {
    // compute
    auto* smComp = static_cast<ShaderModule*>(stages->getComputeModule().get());

    ensureShaderModule(smComp);

    info = smComp->getVulkanShaderModule().getSpvModuleInfo();

    if (info.hasPushConstants) {
      pushConstantMask |= VK_SHADER_STAGE_COMPUTE_BIT;
    }

    stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  } break;
  case igl::ShaderStagesType::Render: {
    auto* smVert = static_cast<ShaderModule*>(stages->getVertexModule().get());
    auto* smFrag = static_cast<ShaderModule*>(stages->getFragmentModule().get());

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

    info = util::mergeReflectionData(infoVert, infoFrag);

    stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  } break;
  case igl::ShaderStagesType::RenderMeshShader: {
    auto* smTask = static_cast<ShaderModule*>(stages->getTaskModule().get());
    auto* smMesh = static_cast<ShaderModule*>(stages->getMeshModule().get());
    auto* smFrag = static_cast<ShaderModule*>(stages->getFragmentModule().get());

    ensureShaderModule(smMesh);
    ensureShaderModule(smFrag);

    const util::SpvModuleInfo& infoMesh = smMesh->getVulkanShaderModule().getSpvModuleInfo();
    const util::SpvModuleInfo& infoFrag = smFrag->getVulkanShaderModule().getSpvModuleInfo();

    if (infoMesh.hasPushConstants) {
      pushConstantMask |= VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    if (infoFrag.hasPushConstants) {
      pushConstantMask |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    info = util::mergeReflectionData(infoMesh, infoFrag);

    stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;

    if (smTask) {
      ensureShaderModule(smTask);

      const util::SpvModuleInfo& infoTask = smTask->getVulkanShaderModule().getSpvModuleInfo();

      if (infoTask.hasPushConstants) {
        pushConstantMask |= VK_SHADER_STAGE_TASK_BIT_EXT;
      }

      info = util::mergeReflectionData(info, infoTask);

      stageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT;
    }
  } break;
  default:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    break;
  };

  if (pushConstantMask) {
    const VkPhysicalDeviceLimits& limits = ctx.getVkPhysicalDeviceProperties().limits;

    constexpr uint32_t kPushConstantsSize = 128;

    if (!IGL_DEBUG_VERIFY(kPushConstantsSize <= limits.maxPushConstantsSize)) {
      IGL_LOG_ERROR("Push constants size exceeded %u (max %u bytes)",
                    kPushConstantsSize,
                    limits.maxPushConstantsSize);
    }

    pushConstantRange = VkPushConstantRange{
        .stageFlags = pushConstantMask,
        .offset = 0u,
        .size = kPushConstantsSize,
    };
  }
}

PipelineState::PipelineState(
    const VulkanContext& ctx,
    IShaderStages* stages,
    std::shared_ptr<ISamplerState> immutableSamplers[IGL_TEXTURE_SAMPLERS_MAX],
    uint32_t isDynamicBufferMask,
    const char* debugName) {
  IGL_DEBUG_ASSERT(stages);

  initializeSpvModuleInfoFromShaderStages(ctx, stages);

  // Create all Vulkan descriptor set layouts for this pipeline

  // 0. Combined image samplers
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info.textures.size());
    for (const auto& t : info.textures) {
      const uint32_t loc = t.bindingLocation;
      bindings.emplace_back(ivkGetDescriptorSetLayoutBinding(
          loc, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stageFlags));
      if (loc < IGL_TEXTURE_SAMPLERS_MAX && immutableSamplers && immutableSamplers[loc]) {
        auto* sampler = static_cast<SamplerState*>(immutableSamplers[loc].get());
        bindings.back().pImmutableSamplers = &ctx.samplers_.get(sampler->sampler_)->vkSampler;
      }
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslCombinedImageSamplers = std::make_unique<VulkanDescriptorSetLayout>(
        ctx,
        VkDescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data(),
        bindingFlags.data(),
        IGL_FORMAT("Descriptor Set Layout (COMBINED_IMAGE_SAMPLER): {}", debugName).c_str());
  }
  // 1. Buffers
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info.buffers.size());
    for (const auto& b : info.buffers) {
      const bool isDynamic = (isDynamicBufferMask & (1ul << b.bindingLocation)) != 0;
      const VkDescriptorType type = b.isStorage
                                        ? (isDynamic ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
                                                     : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                        : (isDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                                     : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
      bindings.emplace_back(
          ivkGetDescriptorSetLayoutBinding(b.bindingLocation, type, 1, stageFlags));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslBuffers = std::make_unique<VulkanDescriptorSetLayout>(
        ctx,
        VkDescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data(),
        bindingFlags.data(),
        IGL_FORMAT("Descriptor Set Layout (BUFFERS): {}", debugName).c_str());
  }
  // 2. Bindless descriptors are managed in VulkanContext

  // 3. Storage images
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info.images.size());
    for (const auto& t : info.images) {
      const uint32_t loc = t.bindingLocation;
      bindings.emplace_back(
          ivkGetDescriptorSetLayoutBinding(loc, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, stageFlags));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslStorageImages = std::make_unique<VulkanDescriptorSetLayout>(
        ctx,
        VkDescriptorSetLayoutCreateFlags{},
        static_cast<uint32_t>(bindings.size()),
        bindings.data(),
        bindingFlags.data(),
        IGL_FORMAT("Descriptor Set Layout (STORAGE_IMAGE): {}", debugName).c_str());
  }
}

VkPipelineLayout PipelineState::getVkPipelineLayout() const {
  IGL_DEBUG_ASSERT(pipelineLayout);

  return pipelineLayout;
}

} // namespace igl::vulkan
