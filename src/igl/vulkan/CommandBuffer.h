/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl {
namespace vulkan {

class Buffer;
class VulkanContext;

class CommandBuffer final : public ICommandBuffer {
 public:
  explicit CommandBuffer(VulkanContext& ctx);
  ~CommandBuffer() override;

  void transitionToShaderReadOnly(ITexture& surface) const override;

  void cmdBindComputePipeline(lvk::ComputePipelineHandle handle) override;
  void cmdDispatchThreadGroups(const Dimensions& threadgroupCount,
                               const Dependencies& deps) override;

  void cmdPushDebugGroupLabel(const char* label, const igl::Color& color) const override;
  void cmdInsertDebugEventLabel(const char* label, const igl::Color& color) const override;
  void cmdPopDebugGroupLabel() const override;

  void cmdBeginRendering(const igl::RenderPass& renderPass,
                         const igl::Framebuffer& desc) override;
  void cmdEndRendering() override;

  void cmdBindViewport(const Viewport& viewport) override;
  void cmdBindScissorRect(const ScissorRect& rect) override;

  void cmdBindRenderPipeline(lvk::RenderPipelineHandle handle) override;
  void cmdBindDepthStencilState(const DepthStencilState& state) override;

  void cmdBindVertexBuffer(uint32_t index,
                           const std::shared_ptr<IBuffer>& buffer,
                           size_t bufferOffset) override;
  void cmdPushConstants(const void* data, size_t size, size_t offset) override;

  void cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) override;
  void cmdDrawIndexed(PrimitiveType primitiveType,
                      size_t indexCount,
                      IndexFormat indexFormat,
                      IBuffer& indexBuffer,
                      size_t indexBufferOffset) override;
  void cmdDrawIndirect(PrimitiveType primitiveType,
                       IBuffer& indirectBuffer,
                       size_t indirectBufferOffset,
                       uint32_t drawCount,
                       uint32_t stride = 0) override;
  void cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                              IndexFormat indexFormat,
                              IBuffer& indexBuffer,
                              IBuffer& indirectBuffer,
                              size_t indirectBufferOffset,
                              uint32_t drawCount,
                              uint32_t stride = 0) override;

  void cmdSetStencilReferenceValues(uint32_t frontValue, uint32_t backValue) override;
  void cmdSetBlendColor(Color color) override;
  void cmdSetDepthBias(float depthBias, float slopeScale, float clamp) override;

  VkCommandBuffer getVkCommandBuffer() const {
    return wrapper_.cmdBuf_;
  }

 private:
  void useComputeTexture(ITexture* texture);
  void bindGraphicsPipeline();

 private:
  friend class Device;

  VulkanContext& ctx_;
  const VulkanImmediateCommands::CommandBufferWrapper& wrapper_;

  igl::Framebuffer framebuffer_ = {};

  VulkanImmediateCommands::SubmitHandle lastSubmitHandle_ = {};

  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;

  bool isRendering_ = false;

  lvk::RenderPipelineHandle currentPipeline_;
  RenderPipelineDynamicState dynamicState_;
};

} // namespace vulkan
} // namespace igl
