/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/UniformAdapter.h>

namespace igl {
class IDepthStencilState;
class IRenderPipelineState;
class ISamplerState;
class ITexture;
struct Dependencies;
namespace opengl {

class RenderCommandAdapter;
class CommandBuffer;

class RenderCommandEncoder final : public IRenderCommandEncoder, public WithContext {
 public:
  static std::unique_ptr<RenderCommandEncoder> create(
      std::shared_ptr<CommandBuffer> commandBuffer,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const Dependencies& dependencies,
      Result* outResult);

  ~RenderCommandEncoder() override;

 private:
  explicit RenderCommandEncoder(std::shared_ptr<CommandBuffer> commandBuffer);
  void beginEncoding(const RenderPassDesc& renderPass,
                     const std::shared_ptr<IFramebuffer>& framebuffer,
                     Result* outResult);

 public:
  void endEncoding() override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;
  void insertDebugEventLabel(const char* label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;

  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  // Binds a non-block uniform (eg. opengl 2.0 shader)
  // The data pointer must remain valid until the commandBuffer's execution has been completed by
  // CommandQueue::submit()
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;
  void bindBuffer(int index,
                  const std::shared_ptr<IBuffer>& buffer,
                  size_t bufferOffset,
                  size_t bufferSize) override;
  void bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset) override;
  void bindIndexBuffer(IBuffer& buffer, IndexFormat format, size_t bufferOffset) override;
  void bindBytes(size_t index, uint8_t target, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset) override;
  void bindSamplerState(size_t index, uint8_t target, ISamplerState* samplerState) override;
  void bindTexture(size_t index, uint8_t target, ITexture* texture) override;

  void draw(size_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
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

 private:
  std::unique_ptr<RenderCommandAdapter> adapter_;
  bool scissorEnabled_ = false;
  int indexType_ = 0;
  void* indexBufferOffset_ = nullptr;
  std::shared_ptr<igl::opengl::Framebuffer> resolveFramebuffer_;
  std::shared_ptr<igl::opengl::Framebuffer> framebuffer_;
};

} // namespace opengl
} // namespace igl
