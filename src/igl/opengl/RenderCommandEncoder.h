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
namespace opengl {

class RenderCommandAdapter;
class CommandBuffer;

class RenderCommandEncoder final : public IRenderCommandEncoder, public WithContext {
 public:
  static std::unique_ptr<RenderCommandEncoder> create(
      const std::shared_ptr<CommandBuffer>& commandBuffer,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      Result* outResult);

  ~RenderCommandEncoder() override;

 private:
  RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer);
  void beginEncoding(const RenderPassDesc& renderPass,
                     const std::shared_ptr<IFramebuffer>& framebuffer,
                     Result* outResult);

 public:
  void endEncoding() override;

  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;
  void insertDebugEventLabel(const std::string& label, const igl::Color& color) const override;
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
                  uint8_t target,
                  const std::shared_ptr<IBuffer>& buffer,
                  size_t bufferOffset) override;
  void bindBytes(size_t index, uint8_t target, const void* data, size_t length) override;
  void bindPushConstants(size_t offset, const void* data, size_t length) override;
  void bindSamplerState(size_t index,
                        uint8_t target,
                        const std::shared_ptr<ISamplerState>& samplerState) override;
  void bindTexture(size_t index, uint8_t target, ITexture* texture) override;

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
                         uint32_t stride) override;
  void multiDrawIndexedIndirect(PrimitiveType primitiveType,
                                IndexFormat indexFormat,
                                IBuffer& indexBuffer,
                                IBuffer& indirectBuffer,
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
  std::shared_ptr<igl::opengl::Framebuffer> resolveFramebuffer_;
  std::shared_ptr<igl::opengl::Framebuffer> framebuffer_;
};

} // namespace opengl
} // namespace igl
