/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "PipelineState.h"

#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanShaderModule.h>

namespace igl::vulkan {

PipelineState::PipelineState(const VulkanContext& ctx,
                             IShaderStages* stages,
                             const char* debugName) {
  if (!stages) {
    return;
  }

  auto* sm = static_cast<igl::vulkan::ShaderModule*>(stages->getComputeModule().get());

  ensureShaderModule(sm);

  info_ = sm->getVulkanShaderModule().getSpvModuleInfo();

  // Create all Vulkan descriptor set layouts for this pipeline

  // 0. Combined image samplers
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(info_.textures.size());
    for (const auto& t : info_.textures) {
      bindings.emplace_back(ivkGetDescriptorSetLayoutBinding(
          t.bindingLocation, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslCombinedImageSamplers_ = std::make_unique<VulkanDescriptorSetLayout>(
        ctx.vf_,
        ctx.getVkDevice(),
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
          b.bindingLocation, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslUniformBuffers_ = std::make_unique<VulkanDescriptorSetLayout>(
        ctx.vf_,
        ctx.getVkDevice(),
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
          b.bindingLocation, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1));
    }
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
    dslStorageBuffers_ = std::make_unique<VulkanDescriptorSetLayout>(
        ctx.vf_,
        ctx.getVkDevice(),
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
