/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/CommandBuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/metal/CommandBuffer.h>

namespace igl {
namespace metal {
class Buffer;

class RenderCommandEncoder final : public IRenderCommandEncoder {
 public:
  static std::unique_ptr<RenderCommandEncoder> create(
      const std::shared_ptr<CommandBuffer>& commandBuffer,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      Result* outResult);

  ~RenderCommandEncoder() override = default;

  void endEncoding() override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;
  void insertDebugEventLabel(const char* label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;

  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  void bindBuffer(int index,
                  const std::shared_ptr<IBuffer>& buffer,
                  size_t bufferOffset,
                  size_t bufferSize) override;
  void bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset) override;
  void bindIndexBuffer(IBuffer& buffer, IndexFormat format, size_t bufferOffset) override;
  void bindBytes(size_t index, uint8_t bindTarget, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset) override;
  void bindSamplerState(size_t index, uint8_t target, ISamplerState* samplerState) override;
  void bindTexture(size_t index, uint8_t target, ITexture* texture) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;

  void draw(PrimitiveType primitiveType,
            size_t vertexStart,
            size_t vertexCount,
            uint32_t instanceCount,
            uint32_t baseInstance) override;
  void draw(size_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t baseInstance) override;
  void drawIndexed(PrimitiveType primitiveType,
                   size_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t vertexOffset,
                   uint32_t baseInstance) override;
  void drawIndexed(size_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t vertexOffset,
                   uint32_t baseInstance) override;
  void multiDrawIndirect(IBuffer& indirectBuffer,
                         size_t indirectBufferOffset,
                         uint32_t drawCount,
                         uint32_t stride) override;
  void multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                size_t indirectBufferOffset,
                                uint32_t drawCount,
                                uint32_t stride) override;

  void setStencilReferenceValue(uint32_t value) override;
  void setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) override;
  void setBlendColor(Color color) override;
  void setDepthBias(float depthBias, float slopeScale, float clamp) override;

  static MTLPrimitiveType convertPrimitiveType(PrimitiveType value);
  static MTLIndexType convertIndexType(IndexFormat value);
  static MTLLoadAction convertLoadAction(LoadAction value);
  static MTLStoreAction convertStoreAction(StoreAction value);
  static MTLClearColor convertClearColor(Color value);

 private:
  explicit RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer);
  void initialize(const std::shared_ptr<CommandBuffer>& commandBuffer,
                  const RenderPassDesc& renderPass,
                  const std::shared_ptr<IFramebuffer>& framebuffer,
                  Result* outResult);

  void bindCullMode(const CullMode& cullMode);
  void bindFrontFacingWinding(const WindingMode& frontFaceWinding);
  void bindPolygonFillMode(const PolygonFillMode& polygonFillMode);

  id<MTLRenderCommandEncoder> encoder_ = nil;
  id<MTLBuffer> indexBuffer_ = nil;
  MTLIndexType indexType_ = MTLIndexTypeUInt32;
  size_t indexBufferOffset_ = 0;
  // 4 KB - page aligned memory for metal managed resource
  static constexpr uint32_t MAX_RECOMMENDED_BYTES = 4 * 1024;

  MTLPrimitiveType metalPrimitive_ = MTLPrimitiveTypeTriangle;
};

} // namespace metal
} // namespace igl
