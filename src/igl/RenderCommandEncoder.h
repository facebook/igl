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
#include <igl/Uniform.h>

namespace igl {

class IBuffer;
class IDepthStencilState;
class IRenderPipelineState;
class ISamplerState;
class ITexture;
struct RenderPassDesc;

struct RenderCommandEncoderDesc {
  // Placeholder
};

namespace BindTarget {
const uint8_t kVertex = 0x0001;
const uint8_t kFragment = 0x0002;
const uint8_t kAllGraphics = 0x0003;
} // namespace BindTarget

class IRenderCommandEncoder : public ICommandEncoder {
 public:
  using ICommandEncoder::ICommandEncoder;

  ~IRenderCommandEncoder() override = default;

  virtual void bindViewport(const Viewport& viewport) = 0;
  virtual void bindScissorRect(const ScissorRect& rect) = 0;

  virtual void bindRenderPipelineState(
      const std::shared_ptr<IRenderPipelineState>& pipelineState) = 0;
  virtual void bindDepthStencilState(
      const std::shared_ptr<IDepthStencilState>& depthStencilState) = 0;

  // Binds the buffer to a shader
  //
  // For metal, the index parameter is the buffer index specified in the shader, for opengl index
  // referes to the location of uniform. The index value can be found by using
  // igl::RenderPipelineState::getIndexByName
  //
  // target is the igl::BindTarget type
  //
  // bufferOffset is the offset into the buffer where the data starts
  virtual void bindBuffer(int index,
                          uint8_t target,
                          const std::shared_ptr<IBuffer>& buffer,
                          size_t bufferOffset) = 0;
  /// Creates and binds a temporary buffer to the specified buffer index.
  virtual void bindBytes(size_t index, uint8_t target, const void* data, size_t length) = 0;
  /// Binds push constant data to the current encoder.
  virtual void bindPushConstants(size_t offset, const void* data, size_t length) = 0;
  virtual void bindSamplerState(size_t index,
                                uint8_t target,
                                const std::shared_ptr<ISamplerState>& samplerState) = 0;

  // For metal, the index parameter is the index in the texture argument table,
  // by the "texture" attribute specified in the shader.
  // For OpenGL, 'index' is the texture unit
  virtual void bindTexture(size_t index, uint8_t target, ITexture* texture) = 0;
  // TODO: keep it here until all the client apps are migrated to the new syntax
  void bindTexture(size_t index, uint8_t target, const std::shared_ptr<ITexture>& texture) {
    bindTexture(index, target, texture.get());
  }

  /// Binds an individual uniform. Exclusively for use when uniform blocks are not supported.
  virtual void bindUniform(const UniformDesc& uniformDesc, const void* data) = 0;

  virtual void draw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) = 0;
  virtual void drawIndexed(PrimitiveType primitiveType,
                           size_t indexCount,
                           IndexFormat indexFormat,
                           IBuffer& indexBuffer,
                           size_t indexBufferOffset) = 0;
  // NOTE: indexBufferOffset parameter is supported in Metal but not OpenGL
  virtual void drawIndexedIndirect(PrimitiveType primitiveType,
                                   IndexFormat indexFormat,
                                   IBuffer& indexBuffer,
                                   IBuffer& indirectBuffer,
                                   size_t indirectBufferOffset) = 0;
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

  virtual void setStencilReferenceValue(uint32_t value) = 0;
  virtual void setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) = 0;
  virtual void setBlendColor(Color color) = 0;
  virtual void setDepthBias(float depthBias, float slopeScale, float clamp) = 0;
};

} // namespace igl
