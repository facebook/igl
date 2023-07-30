/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/LVK.h>
#include <igl/vulkan/Common.h>

#include <string.h>
#include <unordered_map>
#include <vector>

namespace lvk {
namespace vulkan {

class Device;

class alignas(sizeof(uint64_t)) RenderPipelineDynamicState {
  uint32_t topology_ : 4;
  uint32_t depthCompareOp_ : 3;
  uint32_t stencilFrontFailOp_ : 3;
  uint32_t stencilFrontPassOp_ : 3;
  uint32_t stencilFrontDepthFailOp_ : 3;
  uint32_t stencilFrontCompareOp_ : 3;
  uint32_t stencilBackFailOp_ : 3;
  uint32_t stencilBackPassOp_ : 3;
  uint32_t stencilBackDepthFailOp_ : 3;
  uint32_t stencilBackCompareOp_ : 3;

 public:
  uint32_t depthBiasEnable_ : 1;
  uint32_t depthWriteEnable_ : 1;

  RenderPipelineDynamicState() {
    // memset makes sure all padding bits are zero
    memset(this, 0, sizeof(*this));
    topology_ = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    // depth and stencil default state values should be based on DepthStencilStateDesc and
    // StencilStateDesc in graphics/igl/src/igl/DepthStencilState.h
    depthCompareOp_ = VK_COMPARE_OP_ALWAYS;
    stencilFrontFailOp_ = VK_STENCIL_OP_KEEP;
    stencilFrontPassOp_ = VK_STENCIL_OP_KEEP;
    stencilFrontDepthFailOp_ = VK_STENCIL_OP_KEEP;
    stencilFrontCompareOp_ = VK_COMPARE_OP_ALWAYS;
    stencilBackFailOp_ = VK_STENCIL_OP_KEEP;
    stencilBackPassOp_ = VK_STENCIL_OP_KEEP;
    stencilBackDepthFailOp_ = VK_STENCIL_OP_KEEP;
    stencilBackCompareOp_ = VK_COMPARE_OP_ALWAYS;
    depthBiasEnable_ = false;
    depthWriteEnable_ = false;
  }

  VkPrimitiveTopology getTopology() const {
    return static_cast<VkPrimitiveTopology>(topology_);
  }

  void setTopology(VkPrimitiveTopology topology) {
    IGL_ASSERT_MSG((topology & 0xF) == topology, "Invalid VkPrimitiveTopology.");
    topology_ = topology & 0xF;
  }

  VkCompareOp getDepthCompareOp() const {
    return static_cast<VkCompareOp>(depthCompareOp_);
  }

  void setDepthCompareOp(VkCompareOp depthCompareOp) {
    IGL_ASSERT_MSG((depthCompareOp & 0x7) == depthCompareOp, "Invalid VkCompareOp for depth.");
    depthCompareOp_ = depthCompareOp & 0x7;
  }

  VkStencilOp getStencilStateFailOp(bool front) const {
    return static_cast<VkStencilOp>(front ? stencilFrontFailOp_ : stencilBackFailOp_);
  }

  VkStencilOp getStencilStatePassOp(bool front) const {
    return static_cast<VkStencilOp>(front ? stencilFrontPassOp_ : stencilBackPassOp_);
  }

  VkStencilOp getStencilStateDepthFailOp(bool front) const {
    return static_cast<VkStencilOp>(front ? stencilFrontDepthFailOp_ : stencilBackDepthFailOp_);
  }

  VkCompareOp getStencilStateComapreOp(bool front) const {
    return static_cast<VkCompareOp>(front ? stencilFrontCompareOp_ : stencilBackCompareOp_);
  }

  void setStencilStateOps(bool front, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    IGL_ASSERT_MSG((failOp & 0x7) == failOp, "Invalid VkStencilOp for stencil fail.");
    IGL_ASSERT_MSG((passOp & 0x7) == passOp, "Invalid VkStencilOp for stencil pass.");
    IGL_ASSERT_MSG((depthFailOp & 0x7) == depthFailOp, "Invalid VkStencilOp for depth fail.");
    IGL_ASSERT_MSG((compareOp & 0x7) == compareOp, "Invalid VkCompareOp for stencil compare.");

    if (front) {
      stencilFrontFailOp_ = failOp & 0x7;
      stencilFrontPassOp_ = passOp & 0x7;
      stencilFrontDepthFailOp_ = depthFailOp & 0x7;
      stencilFrontCompareOp_ = compareOp & 0x7;
    } else {
      stencilBackFailOp_ = failOp & 0x7;
      stencilBackPassOp_ = passOp & 0x7;
      stencilBackDepthFailOp_ = depthFailOp & 0x7;
      stencilBackCompareOp_ = compareOp & 0x7;
    }
  }

  // comparison operator and hash function for std::unordered_map<>
  bool operator==(const RenderPipelineDynamicState& other) const {
    return *(uint64_t*)this == *(uint64_t*)&other;
  }

  struct HashFunction {
    uint64_t operator()(const RenderPipelineDynamicState& s) const {
      return *(const uint64_t*)&s;
    }
  };
};

static_assert(sizeof(RenderPipelineDynamicState) == sizeof(uint64_t));
static_assert(alignof(RenderPipelineDynamicState) == sizeof(uint64_t));

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

  std::shared_ptr<ShaderStages> shaderStages_;
  RenderPipelineDesc desc_;
  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo_;

  std::vector<VkVertexInputBindingDescription> vkBindings_;
  std::vector<VkVertexInputAttributeDescription> vkAttributes_;

  mutable std::unordered_map<RenderPipelineDynamicState, VkPipeline, RenderPipelineDynamicState::HashFunction> pipelines_;
};

} // namespace vulkan
} // namespace lvk
