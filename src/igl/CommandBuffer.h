/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/Framebuffer.h>
#include <igl/RenderPipelineState.h>

namespace igl {

class IComputePipelineState;
class ISamplerState;
class ITexture;
struct RenderPassDesc;

class ICommandBuffer {
 public:
  virtual ~ICommandBuffer() = default;

  virtual void present(std::shared_ptr<ITexture> surface) const = 0;
  virtual void waitUntilCompleted() = 0;
  virtual void useComputeTexture(const std::shared_ptr<ITexture>& texture) = 0;

  virtual void cmdPushDebugGroupLabel(const std::string& label,
                                      const igl::Color& color = igl::Color(1, 1, 1, 1)) const = 0;
  virtual void cmdInsertDebugEventLabel(const std::string& label,
                                        const igl::Color& color = igl::Color(1, 1, 1, 1)) const = 0;
  virtual void cmdPopDebugGroupLabel() const = 0;

  virtual void cmdBindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) = 0;
  virtual void cmdDispatchThreadGroups(const Dimensions& threadgroupCount) = 0;

  virtual void cmdBeginRenderPass(const RenderPassDesc& renderPass,
                                  const std::shared_ptr<IFramebuffer>& framebuffer) = 0;
  virtual void cmdEndRenderPass() = 0;

  virtual void cmdBindViewport(const Viewport& viewport) = 0;
  virtual void cmdBindScissorRect(const ScissorRect& rect) = 0;

  virtual void cmdBindRenderPipelineState(
      const std::shared_ptr<IRenderPipelineState>& pipelineState) = 0;
  virtual void cmdBindDepthStencilState(const DepthStencilState& state) = 0;

  virtual void cmdBindVertexBuffer(uint32_t index,
                                   const std::shared_ptr<IBuffer>& buffer,
                                   size_t bufferOffset) = 0;
  virtual void cmdPushConstants(size_t offset, const void* data, size_t length) = 0;

  virtual void cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) = 0;
  virtual void cmdDrawIndexed(PrimitiveType primitiveType,
                              size_t indexCount,
                              IndexFormat indexFormat,
                              IBuffer& indexBuffer,
                              size_t indexBufferOffset) = 0;
  virtual void cmdDrawIndirect(PrimitiveType primitiveType,
                               IBuffer& indirectBuffer,
                               size_t indirectBufferOffset,
                               uint32_t drawCount,
                               uint32_t stride = 0) = 0;
  virtual void cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                                      IndexFormat indexFormat,
                                      IBuffer& indexBuffer,
                                      IBuffer& indirectBuffer,
                                      size_t indirectBufferOffset,
                                      uint32_t drawCount,
                                      uint32_t stride = 0) = 0;

  virtual void cmdSetStencilReferenceValues(uint32_t frontValue, uint32_t backValue) = 0;
  virtual void cmdSetBlendColor(Color color) = 0;
  virtual void cmdSetDepthBias(float depthBias, float slopeScale, float clamp) = 0;
};

} // namespace igl
