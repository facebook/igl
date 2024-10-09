/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/util/SpvReflection.h>

namespace igl {
class IShaderStages;
} // namespace igl

namespace igl::vulkan {

class VulkanContext;
class VulkanDescriptorSetLayout;
class VulkanPipelineLayout;

class PipelineState {
 public:
  PipelineState(const VulkanContext& ctx,
                IShaderStages* stages,
                std::shared_ptr<ISamplerState> immutableSamplers[IGL_TEXTURE_SAMPLERS_MAX],
                uint32_t isDynamicBufferMask,
                const char* debugName);
  virtual ~PipelineState() = default;

  PipelineState(const PipelineState&) = delete;
  PipelineState(PipelineState&&) = delete;
  PipelineState& operator=(const PipelineState&) = delete;
  PipelineState& operator=(PipelineState&&) = delete;

  VkPipelineLayout getVkPipelineLayout() const;

  const util::SpvModuleInfo& getSpvModuleInfo() const {
    return info_;
  }

 private:
  void initializeSpvModuleInfoFromShaderStages(const VulkanContext& ctx, IShaderStages* stages);

 public:
  igl::vulkan::util::SpvModuleInfo info_;

  VkPushConstantRange pushConstantRange_ = {};
  VkShaderStageFlags stageFlags_ = 0;

  mutable VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

  // the last seen VkDescriptorSetLayout from VulkanContext::dslBindless_
  mutable VkDescriptorSetLayout lastBindlessVkDescriptorSetLayout_ = VK_NULL_HANDLE;

  std::unique_ptr<VulkanDescriptorSetLayout> dslCombinedImageSamplers_;
  std::unique_ptr<VulkanDescriptorSetLayout> dslBuffers_;
};

} // namespace igl::vulkan
