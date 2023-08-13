/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanPipelineBuilder.h"

#include "lvk/vulkan/VulkanUtils.h"

namespace lvk::vulkan {

uint32_t VulkanPipelineBuilder::numPipelinesCreated_ = 0;

VulkanPipelineBuilder::VulkanPipelineBuilder() :
  vertexInputState_(VkPipelineVertexInputStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  }),
  inputAssembly_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  }),
  rasterizationState_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  }),
  multisampleState_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  }),
  depthStencilState_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_NEVER,
              .compareMask = 0,
              .writeMask = 0,
              .reference = 0,
          },
      .back =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_NEVER,
              .compareMask = 0,
              .writeMask = 0,
              .reference = 0,
          },
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  }) {}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthBiasEnable(bool enable) {
  rasterizationState_.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthWriteEnable(bool enable) {
  depthStencilState_.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthCompareOp(VkCompareOp compareOp) {
  depthStencilState_.depthTestEnable = compareOp != VK_COMPARE_OP_ALWAYS;
  depthStencilState_.depthCompareOp = compareOp;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicState(VkDynamicState state) {
  dynamicStates_.push_back(state);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicStates(const std::vector<VkDynamicState>& states) {
  dynamicStates_.reserve(dynamicStates_.size() + states.size());
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

VulkanPipelineBuilder& VulkanPipelineBuilder::vertexInputState(const VkPipelineVertexInputStateCreateInfo& state) {
  vertexInputState_ = state;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::colorBlendAttachmentStates(std::vector<VkPipelineColorBlendAttachmentState>& states) {
  colorBlendAttachmentStates_ = std::move(states);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::colorAttachmentFormats(std::vector<VkFormat>& formats) {
  colorAttachmentFormats_ = std::move(formats);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthAttachmentFormat(VkFormat format) {
  depthAttachmentFormat_ = format;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilAttachmentFormat(VkFormat format) {
  stencilAttachmentFormat_ = format;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStage(VkPipelineShaderStageCreateInfo stage) {
  shaderStages_.push_back(stage);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStages(const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
  shaderStages_.insert(std::end(shaderStages_), std::begin(stages), std::end(stages));
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilStateOps(VkStencilFaceFlags faceMask,
                                                              VkStencilOp failOp,
                                                              VkStencilOp passOp,
                                                              VkStencilOp depthFailOp,
                                                              VkCompareOp compareOp) {
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

VkResult VulkanPipelineBuilder::build(VkDevice device,
                                      VkPipelineCache pipelineCache,
                                      VkPipelineLayout pipelineLayout,
                                      VkPipeline* outPipeline,
                                      const char* debugName) noexcept {
  const VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = (uint32_t)dynamicStates_.size(),
      .pDynamicStates = dynamicStates_.data(),
  };
  // viewport and scissor can be NULL if the viewport state is dynamic
  // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPipelineViewportStateCreateInfo.html
  const VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };
  const VkPipelineColorBlendStateCreateInfo colorBlendState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = uint32_t(colorBlendAttachmentStates_.size()),
      .pAttachments = colorBlendAttachmentStates_.data(),
  };
  LVK_ASSERT(colorAttachmentFormats_.size() == colorBlendAttachmentStates_.size());

  const VkPipelineRenderingCreateInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = nullptr,
      .colorAttachmentCount = (uint32_t)colorAttachmentFormats_.size(),
      .pColorAttachmentFormats = colorAttachmentFormats_.data(),
      .depthAttachmentFormat = depthAttachmentFormat_,
      .stencilAttachmentFormat = stencilAttachmentFormat_,
  };

  const VkGraphicsPipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingInfo,
      .flags = 0,
      .stageCount = (uint32_t)shaderStages_.size(),
      .pStages = shaderStages_.data(),
      .pVertexInputState = &vertexInputState_,
      .pInputAssemblyState = &inputAssembly_,
      .pTessellationState = nullptr,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizationState_,
      .pMultisampleState = &multisampleState_,
      .pDepthStencilState = &depthStencilState_,
      .pColorBlendState = &colorBlendState,
      .pDynamicState = &dynamicState,
      .layout = pipelineLayout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  const auto result = vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, outPipeline);

  if (!LVK_VERIFY(result == VK_SUCCESS)) {
    return result;
  }

  numPipelinesCreated_++;

  // set debug name
  return lvk::setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)*outPipeline, debugName);
}

} // namespace lvk::vulkan
