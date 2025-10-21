/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderCommandEncoder.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class RenderCommandEncoder final : public IRenderCommandEncoder {
 public:
  ~RenderCommandEncoder() override = default;

  void endEncoding() override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;
  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  void bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset = 0) override;
  void bindIndexBuffer(IBuffer& buffer, IndexFormat format, size_t bufferOffset = 0) override;

  void bindBytes(size_t index, uint8_t target, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset = 0) override;
  void bindSamplerState(size_t index, uint8_t target, ISamplerState* samplerState) override;
  void bindTexture(size_t index, uint8_t target, ITexture* texture) override;
  void bindTexture(size_t index, ITexture* texture) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;

  void draw(size_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t baseInstance = 0) override;
  void drawIndexed(size_t indexCount,
                   uint32_t instanceCount = 1,
                   uint32_t firstIndex = 0,
                   int32_t vertexOffset = 0,
                   uint32_t baseInstance = 0) override;
  void multiDrawIndirect(IBuffer& indirectBuffer,
                        size_t indirectBufferOffset,
                        uint32_t drawCount,
                        uint32_t stride = 0) override;
  void multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                               size_t indirectBufferOffset,
                               uint32_t drawCount,
                               uint32_t stride = 0) override;

  void setStencilReferenceValue(uint32_t value) override;
  void setBlendColor(const Color& color) override;
  void setDepthBias(float depthBias, float slopeScale, float clamp) override;

 private:
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
};

} // namespace igl::d3d12
