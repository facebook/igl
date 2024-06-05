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
  // Vulkan: `index` is the buffer binding index specified in the shader.
  // Metal: `index` is the buffer index specified in the shader.
  // OpenGL: `index` refers to the location of a uniform. The `index` value can be found by using
  // igl::RenderPipelineState::getIndexByName()
  // `bufferOffset` is the offset into the buffer where the data starts
  // `bufferSize` is the size of the buffer to bind used for additional validation (0 means the
  // remaining size starting from `bufferOffset`)
  virtual void bindBuffer(int index,
                          const std::shared_ptr<IBuffer>& buffer,
                          size_t bufferOffset,
                          size_t bufferSize = 0) = 0;
  // On Vulkan and OpenGL: bind a vertex buffer (as in "a buffer with vertices").
  // On Metal: bind any buffer to the vertex stage. Apps should take care there are no 'index'
  // collisions between bindVertexBuffer() and bindBuffer()
  virtual void bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset = 0) = 0;
  virtual void bindIndexBuffer(IBuffer& buffer, IndexFormat format, size_t bufferOffset = 0) = 0;
  /// Creates and binds a temporary buffer to the specified buffer index.
  virtual void bindBytes(size_t index, uint8_t target, const void* data, size_t length) = 0;
  /// Binds push constant data to the current encoder.
  virtual void bindPushConstants(const void* data, size_t length, size_t offset = 0) = 0;
  virtual void bindSamplerState(size_t index, uint8_t target, ISamplerState* samplerState) = 0;

  // For metal, the index parameter is the index in the texture argument table,
  // by the "texture" attribute specified in the shader.
  // For OpenGL, 'index' is the texture unit
  virtual void bindTexture(size_t index, uint8_t target, ITexture* texture) = 0;

  /// Binds an individual uniform. Exclusively for use when uniform blocks are not supported.
  virtual void bindUniform(const UniformDesc& uniformDesc, const void* data) = 0;

  virtual void draw(PrimitiveType primitiveType,
                    size_t vertexStart,
                    size_t vertexCount,
                    uint32_t instanceCount = 1,
                    uint32_t baseInstance = 0) = 0; // old-n-sad
  virtual void draw(size_t vertexCount,
                    uint32_t instanceCount = 1,
                    uint32_t firstVertex = 0,
                    uint32_t baseInstance = 0) {} // new-n-rad
  virtual void drawIndexed(PrimitiveType primitiveType,
                           size_t indexCount,
                           uint32_t instanceCount = 1,
                           uint32_t firstIndex = 0,
                           int32_t vertexOffset = 0,
                           uint32_t baseInstance = 0) = 0; // old-n-sad
  virtual void drawIndexed(size_t indexCount,
                           uint32_t instanceCount = 1,
                           uint32_t firstIndex = 0,
                           int32_t vertexOffset = 0,
                           uint32_t baseInstance = 0) = 0; // new-n-rad
  virtual void multiDrawIndirect(IBuffer& indirectBuffer,
                                 size_t indirectBufferOffset,
                                 uint32_t drawCount = 1,
                                 uint32_t stride = 0) = 0;
  virtual void multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                        size_t indirectBufferOffset,
                                        uint32_t drawCount = 1,
                                        uint32_t stride = 0) = 0;

  virtual void setStencilReferenceValue(uint32_t value) = 0;
  virtual void setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) = 0;
  virtual void setBlendColor(Color color) = 0;
  virtual void setDepthBias(float depthBias, float slopeScale, float clamp) = 0;
};

} // namespace igl
