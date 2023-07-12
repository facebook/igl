/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/CommandEncoder.h>
#include <igl/Common.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/ResourcesBinder.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl {
namespace vulkan {

class RenderCommandEncoder : public IRenderCommandEncoder {
 public:
  static std::unique_ptr<RenderCommandEncoder> create(
      const std::shared_ptr<CommandBuffer>& commandBuffer,
      const VulkanContext& ctx,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      Result* outResult);

  ~RenderCommandEncoder() override {
    IGL_ASSERT(!isEncoding_); // did you forget to call endEncoding()?
    endEncoding();
  }

  void endEncoding() override;

  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;
  void insertDebugEventLabel(const std::string& label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;

  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  void bindBuffer(int index,
                  uint8_t target,
                  const std::shared_ptr<IBuffer>& buffer,
                  size_t bufferOffset) override;
  void bindBytes(size_t index, uint8_t target, const void* data, size_t length) override;
  void bindPushConstants(size_t offset, const void* data, size_t length) override;
  void bindSamplerState(size_t index,
                        uint8_t target,
                        const std::shared_ptr<ISamplerState>& samplerState) override;

  void bindTexture(size_t index, uint8_t target, ITexture* texture) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;

  void draw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) override;
  void drawIndexed(PrimitiveType primitiveType,
                   size_t indexCount,
                   IndexFormat indexFormat,
                   IBuffer& indexBuffer,
                   size_t indexBufferOffset) override;
  void drawIndexedIndirect(PrimitiveType primitiveType,
                           IndexFormat indexFormat,
                           IBuffer& indexBuffer,
                           IBuffer& indirectBuffer,
                           size_t indirectBufferOffset) override;
  void multiDrawIndirect(PrimitiveType primitiveType,
                         IBuffer& indirectBuffer,
                         size_t indirectBufferOffset,
                         uint32_t drawCount,
                         uint32_t stride = 0) override;
  void multiDrawIndexedIndirect(PrimitiveType primitiveType,
                                IndexFormat indexFormat,
                                IBuffer& indexBuffer,
                                IBuffer& indirectBuffer,
                                size_t indirectBufferOffset,
                                uint32_t drawCount,
                                uint32_t stride = 0) override;

  void setStencilReferenceValue(uint32_t value) override;
  void setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) override;
  void setBlendColor(Color color) override;
  void setDepthBias(float depthBias, float slopeScale, float clamp) override;

  VkCommandBuffer getVkCommandBuffer() const {
    return cmdBuffer_;
  }

  igl::vulkan::ResourcesBinder& binder() {
    return binder_;
  }

  bool setDrawCallCountEnabled(bool value);

 private:
  void bindPipeline();

 private:
  const VulkanContext& ctx_;
  VkCommandBuffer cmdBuffer_ = VK_NULL_HANDLE;
  bool isEncoding_ = false;
  bool hasDepthAttachment_ = false;
  std::shared_ptr<IFramebuffer> framebuffer_;

  igl::vulkan::ResourcesBinder binder_;

  std::shared_ptr<igl::IRenderPipelineState> currentPipeline_ = nullptr;
  RenderPipelineDynamicState dynamicState_;

  /* Used to increment the draw call count. Should either be 0 or 1
   *  0: When draw call count is disabled during auxiliary draw calls (shader debugging)
   *  1: All other times */
  uint32_t drawCallCountEnabled_ = 1u;

 private:
  RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                       const VulkanContext& ctx);

  void initialize(const RenderPassDesc& renderPass,
                  const std::shared_ptr<IFramebuffer>& framebuffer,
                  Result* outResult);
};

} // namespace vulkan
} // namespace igl
