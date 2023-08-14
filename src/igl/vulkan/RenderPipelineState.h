/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/vulkan/VulkanUtils.h>

#include <string.h>
#include <unordered_map>
#include <vector>

namespace lvk {
namespace vulkan {

class Device;

class alignas(sizeof(uint32_t)) RenderPipelineDynamicState {
  uint32_t topology_ : 4;
  uint32_t depthCompareOp_ : 3;

 public:
  uint32_t depthBiasEnable_ : 1;
  uint32_t depthWriteEnable_ : 1;

  RenderPipelineDynamicState() {
    // memset makes sure all padding bits are zero
    memset(this, 0, sizeof(*this));
    topology_ = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    depthCompareOp_ = VK_COMPARE_OP_ALWAYS;
    depthBiasEnable_ = VK_FALSE;
    depthWriteEnable_ = VK_FALSE;
  }

  VkPrimitiveTopology getTopology() const {
    return static_cast<VkPrimitiveTopology>(topology_);
  }

  void setTopology(VkPrimitiveTopology topology) {
    LVK_ASSERT_MSG((topology & 0xF) == topology, "Invalid VkPrimitiveTopology.");
    topology_ = topology & 0xF;
  }

  VkCompareOp getDepthCompareOp() const {
    return static_cast<VkCompareOp>(depthCompareOp_);
  }

  void setDepthCompareOp(VkCompareOp depthCompareOp) {
    LVK_ASSERT_MSG((depthCompareOp & 0x7) == depthCompareOp, "Invalid VkCompareOp for depth.");
    depthCompareOp_ = depthCompareOp & 0x7;
  }

  // comparison operator and hash function for std::unordered_map<>
  bool operator==(const RenderPipelineDynamicState& other) const {
    return *(uint32_t*)this == *(uint32_t*)&other;
  }

  struct HashFunction {
    uint64_t operator()(const RenderPipelineDynamicState& s) const {
      return *(const uint32_t*)&s;
    }
  };
};

static_assert(sizeof(RenderPipelineDynamicState) == sizeof(uint32_t));
static_assert(alignof(RenderPipelineDynamicState) == sizeof(uint32_t));

class RenderPipelineState final {
 public:
  RenderPipelineState() = default;
  RenderPipelineState(lvk::vulkan::Device* device, const RenderPipelineDesc& desc);
  ~RenderPipelineState();

  RenderPipelineState(const RenderPipelineState&) = delete;
  RenderPipelineState& operator=(const RenderPipelineState&) = delete;

  RenderPipelineState(RenderPipelineState&& other);
  RenderPipelineState& operator=(RenderPipelineState&& other);

  VkPipeline getVkPipeline(const RenderPipelineDynamicState& dynamicState) const;

  const RenderPipelineDesc& getRenderPipelineDesc() const {
    return desc_;
  }

 private:
  lvk::vulkan::Device* device_ = nullptr;

  RenderPipelineDesc desc_;
  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo_;

  std::vector<VkVertexInputBindingDescription> vkBindings_;
  std::vector<VkVertexInputAttributeDescription> vkAttributes_;

  mutable std::unordered_map<RenderPipelineDynamicState, VkPipeline, RenderPipelineDynamicState::HashFunction> pipelines_;
};

class VulkanPipelineBuilder final {
 public:
  VulkanPipelineBuilder();
  ~VulkanPipelineBuilder() = default;

  VulkanPipelineBuilder& depthBiasEnable(bool enable);
  VulkanPipelineBuilder& depthWriteEnable(bool enable);
  VulkanPipelineBuilder& depthCompareOp(VkCompareOp compareOp);
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
  VulkanPipelineBuilder& stencilMasks(VkStencilFaceFlags faceMask, uint32_t compareMask, uint32_t writeMask, uint32_t reference);
  VulkanPipelineBuilder& cullMode(VkCullModeFlags mode);
  VulkanPipelineBuilder& frontFace(VkFrontFace mode);
  VulkanPipelineBuilder& polygonMode(VkPolygonMode mode);
  VulkanPipelineBuilder& vertexInputState(const VkPipelineVertexInputStateCreateInfo& state);
  VulkanPipelineBuilder& colorBlendAttachmentStates(std::vector<VkPipelineColorBlendAttachmentState>& states);
  VulkanPipelineBuilder& colorAttachmentFormats(std::vector<VkFormat>& formats);
  VulkanPipelineBuilder& depthAttachmentFormat(VkFormat format);
  VulkanPipelineBuilder& stencilAttachmentFormat(VkFormat format);

  VkResult build(VkDevice device,
                 VkPipelineCache pipelineCache,
                 VkPipelineLayout pipelineLayout,
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
  std::vector<VkFormat> colorAttachmentFormats_;
  VkFormat depthAttachmentFormat_ = VK_FORMAT_UNDEFINED;
  VkFormat stencilAttachmentFormat_ = VK_FORMAT_UNDEFINED;
  static uint32_t numPipelinesCreated_;
};

} // namespace vulkan
} // namespace lvk
