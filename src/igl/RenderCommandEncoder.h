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

namespace igl {

class IBuffer;
class IRenderPipelineState;
class ISamplerState;
class ITexture;
struct DepthStencilState;
struct RenderPassDesc;

class IRenderCommandEncoder : public ICommandEncoder {
 public:
  using ICommandEncoder::ICommandEncoder;

  ~IRenderCommandEncoder() override = default;

  virtual void bindViewport(const Viewport& viewport) = 0;
  virtual void bindScissorRect(const ScissorRect& rect) = 0;

  virtual void bindRenderPipelineState(
      const std::shared_ptr<IRenderPipelineState>& pipelineState) = 0;
  virtual void bindDepthStencilState(const DepthStencilState& state) = 0;

  virtual void bindVertexBuffer(uint32_t index,
                                const std::shared_ptr<IBuffer>& buffer,
                                size_t bufferOffset) = 0;
  virtual void bindPushConstants(size_t offset, const void* data, size_t length) = 0;

  virtual void draw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) = 0;
  virtual void drawIndexed(PrimitiveType primitiveType,
                           size_t indexCount,
                           IndexFormat indexFormat,
                           IBuffer& indexBuffer,
                           size_t indexBufferOffset) = 0;
  virtual void multiDrawIndirect(PrimitiveType primitiveType,
                                 IBuffer& indirectBuffer,
                                 size_t indirectBufferOffset,
                                 uint32_t drawCount,
                                 uint32_t stride = 0) = 0;
  virtual void multiDrawIndexedIndirect(PrimitiveType primitiveType,
                                        IndexFormat indexFormat,
                                        IBuffer& indexBuffer,
                                        IBuffer& indirectBuffer,
                                        size_t indirectBufferOffset,
                                        uint32_t drawCount,
                                        uint32_t stride = 0) = 0;

  virtual void setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) = 0;
  virtual void setBlendColor(Color color) = 0;
  virtual void setDepthBias(float depthBias, float slopeScale, float clamp) = 0;
};

} // namespace igl
