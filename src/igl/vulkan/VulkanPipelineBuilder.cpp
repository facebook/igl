/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanPipelineBuilder.h"

namespace igl::vulkan {

uint32_t VulkanPipelineBuilder::numPipelinesCreated_ = 0;
uint32_t VulkanComputePipelineBuilder::numPipelinesCreated_ = 0;

VulkanPipelineBuilder::VulkanPipelineBuilder() :
  vertexInputState_(ivkGetPipelineVertexInputStateCreateInfo_Empty()),
  inputAssembly_(
      ivkGetPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)),
  rasterizationState_(
      ivkGetPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE)),
  multisampleState_(ivkGetPipelineMultisampleStateCreateInfo_Empty()),
  depthStencilState_(ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests()) {}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthBiasEnable(bool enable) {
  rasterizationState_.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthWriteEnable(bool enable) {
  depthStencilState_.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthCompareOp(VkCompareOp compareOp) {
  depthStencilState_.depthTestEnable = static_cast<VkBool32>(compareOp != VK_COMPARE_OP_ALWAYS);
  depthStencilState_.depthCompareOp = compareOp;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicState(VkDynamicState state) {
  dynamicStates_.push_back(state);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicStates(
    const std::vector<VkDynamicState>& states) {
  dynamicStates_.insert(std::end(dynamicStates_), std::begin(states), std::end(states));
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::primitiveTopology(VkPrimitiveTopology topology) {
  inputAssembly_.topology = topology;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::rasterizationSamples(VkSampleCountFlagBits samples) {
  multisampleState_.rasterizationSamples = samples;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::cullMode(VkCullModeFlags mode) {
  rasterizationState_.cullMode = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::frontFace(VkFrontFace mode) {
  rasterizationState_.frontFace = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::polygonMode(VkPolygonMode mode) {
  rasterizationState_.polygonMode = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::vertexInputState(
    const VkPipelineVertexInputStateCreateInfo& state) {
  vertexInputState_ = state;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::colorBlendAttachmentStates(
    std::vector<VkPipelineColorBlendAttachmentState>& states) {
  colorBlendAttachmentStates_ = std::move(states);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStage(VkPipelineShaderStageCreateInfo stage) {
  shaderStages_.push_back(stage);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStages(
    const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
  shaderStages_.insert(std::end(shaderStages_), std::begin(stages), std::end(stages));
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilStateOps(VkStencilFaceFlags faceMask,
                                                              VkStencilOp failOp,
                                                              VkStencilOp passOp,
                                                              VkStencilOp depthFailOp,
                                                              VkCompareOp compareOp) {
  depthStencilState_.stencilTestEnable =
      static_cast<VkBool32>(static_cast<bool>(depthStencilState_.stencilTestEnable) ||
                            failOp != VK_STENCIL_OP_KEEP || passOp != VK_STENCIL_OP_KEEP ||
                            depthFailOp != VK_STENCIL_OP_KEEP || compareOp != VK_COMPARE_OP_ALWAYS);

  if (faceMask & VK_STENCIL_FACE_FRONT_BIT) {
    VkStencilOpState& front = depthStencilState_.front;
    front.failOp = failOp;
    front.passOp = passOp;
    front.depthFailOp = depthFailOp;
    front.compareOp = compareOp;
  }
  if (faceMask & VK_STENCIL_FACE_BACK_BIT) {
    VkStencilOpState& back = depthStencilState_.back;
    back.failOp = failOp;
    back.passOp = passOp;
    back.depthFailOp = depthFailOp;
    back.compareOp = compareOp;
  }
  return *this;
}

VkResult VulkanPipelineBuilder::build(const VulkanFunctionTable& vf,
                                      VkDevice device,
                                      VkPipelineCache pipelineCache,
                                      VkPipelineLayout pipelineLayout,
                                      VkRenderPass renderPass,
                                      VkPipeline* outPipeline,
                                      const char* debugName) noexcept {
  const VkPipelineDynamicStateCreateInfo dynamicState =
      ivkGetPipelineDynamicStateCreateInfo((uint32_t)dynamicStates_.size(), dynamicStates_.data());
  // viewport and scissor are always dynamic
  const VkPipelineViewportStateCreateInfo viewportState =
      ivkGetPipelineViewportStateCreateInfo(nullptr, nullptr);
  const VkPipelineColorBlendStateCreateInfo colorBlendState =
      ivkGetPipelineColorBlendStateCreateInfo(uint32_t(colorBlendAttachmentStates_.size()),
                                              colorBlendAttachmentStates_.data());

  const auto result = ivkCreateGraphicsPipeline(&vf,
                                                device,
                                                pipelineCache,
                                                (uint32_t)shaderStages_.size(),
                                                shaderStages_.data(),
                                                &vertexInputState_,
                                                &inputAssembly_,
                                                nullptr,
                                                &viewportState,
                                                &rasterizationState_,
                                                &multisampleState_,
                                                &depthStencilState_,
                                                &colorBlendState,
                                                &dynamicState,
                                                pipelineLayout,
                                                renderPass,
                                                outPipeline);

  if (!IGL_DEBUG_VERIFY(result == VK_SUCCESS)) {
    return result;
  }

  numPipelinesCreated_++;

  // set debug name
  return ivkSetDebugObjectName(
      &vf, device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)*outPipeline, debugName);
}

VulkanComputePipelineBuilder& VulkanComputePipelineBuilder::shaderStage(
    VkPipelineShaderStageCreateInfo stage) {
  shaderStage_ = stage;
  return *this;
}

VkResult VulkanComputePipelineBuilder::build(const VulkanFunctionTable& vf,
                                             VkDevice device,
                                             VkPipelineCache pipelineCache,
                                             VkPipelineLayout pipelineLayout,
                                             VkPipeline* outPipeline,
                                             const char* debugName) noexcept {
  const VkResult result = ivkCreateComputePipeline(
      &vf, device, pipelineCache, &shaderStage_, pipelineLayout, outPipeline);

  if (!IGL_DEBUG_VERIFY(result == VK_SUCCESS)) {
    return result;
  }

  numPipelinesCreated_++;

  // set debug name
  return ivkSetDebugObjectName(
      &vf, device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)*outPipeline, debugName);
}

} // namespace igl::vulkan
