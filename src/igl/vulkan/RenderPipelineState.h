/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderPipelineState.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/PipelineState.h>
#include <igl/vulkan/RenderPipelineReflection.h>
#include <unordered_map>

namespace igl::vulkan {

class Device;

/// @brief This class stores all mutable pipeline parameters as member variables and serves as a
/// hash key for the `RenderPipelineState` class
class alignas(sizeof(uint64_t)) RenderPipelineDynamicState {
  uint32_t depthCompareOp_ : 3;

  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilFrontFailOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilFrontPassOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilFrontDepthFailOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilFrontCompareOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilBackFailOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilBackPassOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilBackDepthFailOp_ : 3;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t stencilBackCompareOp_ : 3;

 public:
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t renderPassIndex_ : 8;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t depthBiasEnable_ : 1;
  // Ignore modernize-use-default-member-init
  // @lint-ignore CLANGTIDY
  uint32_t depthWriteEnable_ : 1;

  RenderPipelineDynamicState() {
    // memset makes sure all padding bits are zero
    std::memset(this, 0, sizeof(*this));
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
    renderPassIndex_ = 0;
    depthBiasEnable_ = false;
    depthWriteEnable_ = false;
  }

  [[nodiscard]] VkCompareOp getDepthCompareOp() const {
    return static_cast<VkCompareOp>(depthCompareOp_);
  }

  void setDepthCompareOp(VkCompareOp depthCompareOp) {
    IGL_DEBUG_ASSERT((depthCompareOp & 0x7) == depthCompareOp, "Invalid VkCompareOp for depth.");
    depthCompareOp_ = depthCompareOp & 0x7;
  }

  [[nodiscard]] VkStencilOp getStencilStateFailOp(bool front) const {
    return static_cast<VkStencilOp>(front ? stencilFrontFailOp_ : stencilBackFailOp_);
  }

  [[nodiscard]] VkStencilOp getStencilStatePassOp(bool front) const {
    return static_cast<VkStencilOp>(front ? stencilFrontPassOp_ : stencilBackPassOp_);
  }

  [[nodiscard]] VkStencilOp getStencilStateDepthFailOp(bool front) const {
    return static_cast<VkStencilOp>(front ? stencilFrontDepthFailOp_ : stencilBackDepthFailOp_);
  }

  [[nodiscard]] VkCompareOp getStencilStateCompareOp(bool front) const {
    return static_cast<VkCompareOp>(front ? stencilFrontCompareOp_ : stencilBackCompareOp_);
  }

  void setStencilStateOps(bool front,
                          VkStencilOp failOp,
                          VkStencilOp passOp,
                          VkStencilOp depthFailOp,
                          VkCompareOp compareOp) {
    IGL_DEBUG_ASSERT((failOp & 0x7) == failOp, "Invalid VkStencilOp for stencil fail.");
    IGL_DEBUG_ASSERT((passOp & 0x7) == passOp, "Invalid VkStencilOp for stencil pass.");
    IGL_DEBUG_ASSERT((depthFailOp & 0x7) == depthFailOp, "Invalid VkStencilOp for depth fail.");
    IGL_DEBUG_ASSERT((compareOp & 0x7) == compareOp, "Invalid VkCompareOp for stencil compare.");

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

/** @brief Implements the igl::IRenderPipelineState interface. In Vulkan, certain render parameters
 * belong to a pipeline, which is immutable, and changing parameters is not possible once a
 * pipeline has been created. IGL, on the other hand, allows some pipeline parameters to be changed.
 * This class manages a hash map internally that automatically tracks all pipeline's instances that
 * are created based on the original parameters provided during the instantiation of the class. A
 * Vulkan pipeline object can be retrieved with the `getVkPipeline()` method by providing its
 * mutable parameters. If a pipeline doesn't exist with those parameters, one is created and
 * returned. Otherwise an existing pipeline with those settings is returned. This class also tracks
 * the pipeline layout in the context. If a pipeline layout change is detected, this class purges
 * all the pipelines that have been created so far.
 */
class RenderPipelineState final : public IRenderPipelineState, public PipelineState {
 public:
  /** @brief Caches the render pipeline parameters passed in `desc` for later use. A pipeline isn't
   * realized until `getVkPipeline()` is called and all mutable parameters are provided.
   */
  RenderPipelineState(const igl::vulkan::Device& device, RenderPipelineDesc desc);
  ~RenderPipelineState() override;

  /** @brief Creates a pipeline with the base parameters provided during construction and all
   * mutable ones provided in the `dynamicState` parameter. If a pipeline layout change is detected,
   * all cached pipelines are discarded.
   */
  VkPipeline getVkPipeline(const RenderPipelineDynamicState& dynamicState) const;

 private:
  friend class Device;

  int getIndexByName(const igl::NameHandle& name, ShaderStage stage) const override;
  int getIndexByName(const std::string& name, ShaderStage stage) const override;

  std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() override;
  void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) override;

 private:
  const igl::vulkan::Device& device_;

  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo_{};

  std::vector<VkVertexInputBindingDescription> vkBindings_;
  std::array<VkVertexInputAttributeDescription, IGL_VERTEX_ATTRIBUTES_MAX> vkAttributes_{};

  // This is empty for now.
  std::shared_ptr<RenderPipelineReflection> reflection_;

  mutable std::unordered_map<RenderPipelineDynamicState,
                             VkPipeline,
                             RenderPipelineDynamicState::HashFunction>
      pipelines_;
};

} // namespace igl::vulkan
