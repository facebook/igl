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

namespace lvk {
namespace vulkan {

class VulkanContext;

class CommandBuffer final : public ICommandBuffer {
 public:
  CommandBuffer() = default;
  explicit CommandBuffer(VulkanContext* ctx);
  ~CommandBuffer() override;

  CommandBuffer& operator=(CommandBuffer&& other) = default;

  void transitionToShaderReadOnly(TextureHandle surface) const override;

  void cmdBindComputePipeline(lvk::ComputePipelineHandle handle) override;
  void cmdDispatchThreadGroups(const Dimensions& threadgroupCount, const Dependencies& deps) override;

  void cmdPushDebugGroupLabel(const char* label, uint32_t colorRGBA) const override;
  void cmdInsertDebugEventLabel(const char* label, uint32_t colorRGBA) const override;
  void cmdPopDebugGroupLabel() const override;

  void cmdBeginRendering(const lvk::RenderPass& renderPass, const lvk::Framebuffer& desc) override;
  void cmdEndRendering() override;

  void cmdBindViewport(const Viewport& viewport) override;
  void cmdBindScissorRect(const ScissorRect& rect) override;

  void cmdBindRenderPipeline(lvk::RenderPipelineHandle handle) override;
  void cmdBindDepthStencilState(const DepthStencilState& state) override;

  void cmdBindVertexBuffer(uint32_t index, BufferHandle buffer, size_t bufferOffset) override;
  void cmdPushConstants(const void* data, size_t size, size_t offset) override;

  void cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) override;
  void cmdDrawIndexed(PrimitiveType primitiveType,
                      size_t indexCount,
                      IndexFormat indexFormat,
                      BufferHandle indexBuffer,
                      size_t indexBufferOffset) override;
  void cmdDrawIndirect(PrimitiveType primitiveType,
                       BufferHandle indirectBuffer,
                       size_t indirectBufferOffset,
                       uint32_t drawCount,
                       uint32_t stride = 0) override;
  void cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                              IndexFormat indexFormat,
                              BufferHandle indexBuffer,
                              BufferHandle indirectBuffer,
                              size_t indirectBufferOffset,
                              uint32_t drawCount,
                              uint32_t stride = 0) override;

  void cmdSetStencilReferenceValues(uint32_t frontValue, uint32_t backValue) override;
  void cmdSetBlendColor(const float color[4]) override;
  void cmdSetDepthBias(float depthBias, float slopeScale, float clamp) override;

 private:
  void useComputeTexture(TextureHandle texture);
  void bindGraphicsPipeline();

 private:
  friend class Device;

  VulkanContext* ctx_ = nullptr;
  const VulkanImmediateCommands::CommandBufferWrapper* wrapper_ = nullptr;

  lvk::Framebuffer framebuffer_ = {};

  VulkanImmediateCommands::SubmitHandle lastSubmitHandle_ = {};

  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;

  bool isRendering_ = false;

  lvk::RenderPipelineHandle currentPipeline_ = {};
  RenderPipelineDynamicState dynamicState_ = {};
};

} // namespace vulkan
} // namespace lvk
