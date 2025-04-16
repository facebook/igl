/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

class VulkanPipelineBuilder final {
 public:
  VulkanPipelineBuilder();
  ~VulkanPipelineBuilder() = default;

  VulkanPipelineBuilder& depthBiasEnable(bool enable);
  VulkanPipelineBuilder& depthWriteEnable(bool enable);
  VulkanPipelineBuilder& depthCompareOp(VkCompareOp compareOp, bool writeDepthEnable);
  VulkanPipelineBuilder& dynamicState(VkDynamicState state);
  VulkanPipelineBuilder& dynamicStates(const std::vector<VkDynamicState>& states);
  VulkanPipelineBuilder& primitiveTopology(VkPrimitiveTopology topology);
  VulkanPipelineBuilder& rasterizationSamples(VkSampleCountFlagBits samples);
  VulkanPipelineBuilder& shaderStage(VkPipelineShaderStageCreateInfo stage);
  VulkanPipelineBuilder& shaderStages(const std::vector<VkPipelineShaderStageCreateInfo>& stages);
  VulkanPipelineBuilder& stencilStateOps(VkStencilFaceFlags faceMask,
                                         VkStencilOp failOp,
                                         VkStencilOp passOp,
                                         VkStencilOp depthFailOp,
                                         VkCompareOp compareOp);
  VulkanPipelineBuilder& cullMode(VkCullModeFlags mode);
  VulkanPipelineBuilder& frontFace(VkFrontFace mode);
  VulkanPipelineBuilder& polygonMode(VkPolygonMode mode);
  VulkanPipelineBuilder& vertexInputState(const VkPipelineVertexInputStateCreateInfo& state);
  VulkanPipelineBuilder& colorBlendAttachmentStates(
      std::vector<VkPipelineColorBlendAttachmentState>& states);

  [[nodiscard]] VkResult build(const VulkanFunctionTable& vf,
                               VkDevice device,
                               VkPipelineCache pipelineCache,
                               VkPipelineLayout pipelineLayout,
                               VkRenderPass renderPass,
                               VkPipeline* outPipeline,
                               const char* debugName = nullptr) noexcept;

  static uint32_t getNumPipelinesCreated() {
    return numPipelinesCreated_;
  }

 private:
  std::vector<VkDynamicState> dynamicStates_;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages_;
  VkPipelineVertexInputStateCreateInfo vertexInputState_;
  VkPipelineInputAssemblyStateCreateInfo inputAssembly_;
  VkPipelineRasterizationStateCreateInfo rasterizationState_;
  VkPipelineMultisampleStateCreateInfo multisampleState_;
  VkPipelineDepthStencilStateCreateInfo depthStencilState_;
  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates_;
  static uint32_t numPipelinesCreated_;
};

class VulkanComputePipelineBuilder final {
 public:
  VulkanComputePipelineBuilder() = default;
  ~VulkanComputePipelineBuilder() = default;

  VulkanComputePipelineBuilder& shaderStage(VkPipelineShaderStageCreateInfo stage);

  VkResult build(const VulkanFunctionTable& vf,
                 VkDevice device,
                 VkPipelineCache pipelineCache,
                 VkPipelineLayout pipelineLayout,
                 VkPipeline* outPipeline,
                 const char* debugName = nullptr) noexcept;

  static uint32_t getNumPipelinesCreated() {
    return numPipelinesCreated_;
  }

 private:
  VkPipelineShaderStageCreateInfo shaderStage_;
  static uint32_t numPipelinesCreated_;
};

} // namespace igl::vulkan
