/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/RenderCommandEncoder.h>

#include <igl/opengl/Buffer.h>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/DepthStencilState.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderCommandAdapter.h>
#include <igl/opengl/RenderPipelineState.h>
#include <igl/opengl/SamplerState.h>
#include <igl/opengl/UniformAdapter.h>
#include <igl/opengl/VertexInputState.h>

namespace igl::opengl {

namespace {
GLenum toGlPrimitive(PrimitiveType t) {
  GLenum result = GL_TRIANGLES;
  switch (t) {
  case PrimitiveType::Point:
    result = GL_POINTS;
    break;
  case PrimitiveType::Line:
    result = GL_LINES;
    break;
  case PrimitiveType::LineStrip:
    result = GL_LINE_STRIP;
    break;
  case PrimitiveType::Triangle:
    break;
  case PrimitiveType::TriangleStrip:
    result = GL_TRIANGLE_STRIP;
    break;
  }
  return result;
}

GLenum toGlType(IndexFormat format) {
  switch (format) {
  case IndexFormat::UInt8:
    return GL_UNSIGNED_BYTE;
  case IndexFormat::UInt16:
    return GL_UNSIGNED_SHORT;
  case IndexFormat::UInt32:
    return GL_UNSIGNED_INT;
  }
  IGL_UNREACHABLE_RETURN(GL_UNSIGNED_INT)
}

uint8_t getIndexByteSize(GLenum indexType) {
  switch (indexType) {
  case GL_UNSIGNED_BYTE:
    return 1u;
  case GL_UNSIGNED_SHORT:
    return 2u;
  case GL_UNSIGNED_INT:
    return 4u;
  }
  IGL_UNREACHABLE_RETURN(4u)
}

} // namespace

RenderCommandEncoder::RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer) :
  IRenderCommandEncoder(commandBuffer),
  WithContext(static_cast<CommandBuffer&>(getCommandBuffer()).getContext()) {}

std::unique_ptr<RenderCommandEncoder> RenderCommandEncoder::create(
    const std::shared_ptr<CommandBuffer>& commandBuffer,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& /*dependencies*/,
    Result* outResult) {
  if (!commandBuffer) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "commandBuffer was null");
    return {};
  }

  std::unique_ptr<RenderCommandEncoder> newEncoder(new RenderCommandEncoder(commandBuffer));
  newEncoder->beginEncoding(renderPass, framebuffer, outResult);
  return newEncoder;
}

RenderCommandEncoder::~RenderCommandEncoder() = default;

void RenderCommandEncoder::beginEncoding(const RenderPassDesc& renderPass,
                                         const std::shared_ptr<IFramebuffer>& framebuffer,
                                         Result* outResult) {
  // Save caller state
  auto& context = getContext();

  scissorEnabled_ = (context.isEnabled(GL_SCISSOR_TEST) != 0u);
  context.disable(GL_SCISSOR_TEST); // only turn on if bindScissorRect is called

  auto& pool = context.getAdapterPool();
  if (pool.empty()) {
    Result result;
    adapter_ = RenderCommandAdapter::create(context, renderPass, framebuffer, &result);
    if (!result.isOk()) {
      if (outResult) {
        *outResult = result;
      }
      return;
    }
  } else {
    Result result;
    adapter_ = std::move(pool[pool.size() - 1]);
    pool.pop_back();
    adapter_->initialize(renderPass, framebuffer, &result);
    if (!result.isOk()) {
      if (outResult) {
        *outResult = result;
      }
      return;
    }
  }
  framebuffer_ = std::static_pointer_cast<Framebuffer>(framebuffer);
  resolveFramebuffer_ = framebuffer_->getResolveFramebuffer();
  Result::setOk(outResult);
}

void RenderCommandEncoder::endEncoding() {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    // Restore caller state
    getContext().setEnabled(scissorEnabled_, GL_SCISSOR_TEST);

    // Disable depthBias
    getContext().setEnabled(false, GL_POLYGON_OFFSET_FILL);
    adapter_->setDepthBias(0.0f, 0.0f, 0.0f);

    adapter_->endEncoding();
    getContext().getAdapterPool().push_back(std::move(adapter_));

    if (resolveFramebuffer_) {
      Result outResult;
      auto width = 0;
      auto height = 0;
      auto mask = 0;
      auto sizeMatch = true;
      if (resolveFramebuffer_->getColorAttachment(0)) {
        const auto colorDimensions = resolveFramebuffer_->getColorAttachment(0)->getDimensions();
        mask |= GL_COLOR_BUFFER_BIT;
        width = static_cast<int>(colorDimensions.width);
        height = static_cast<int>(colorDimensions.height);
      }
      if (resolveFramebuffer_->getDepthAttachment()) {
        mask |= GL_DEPTH_BUFFER_BIT;
        const auto depthDimensions = resolveFramebuffer_->getDepthAttachment()->getDimensions();
        if (width != 0 && static_cast<int>(depthDimensions.width) != width) {
          sizeMatch = false;
        }
        if (height != 0 && static_cast<int>(depthDimensions.height) != height) {
          sizeMatch = false;
        }
        width = static_cast<int>(depthDimensions.width);
        height = static_cast<int>(depthDimensions.height);
      }
      if (resolveFramebuffer_->getStencilAttachment()) {
        const auto stencilDimensions = resolveFramebuffer_->getStencilAttachment()->getDimensions();
        mask |= GL_STENCIL_BUFFER_BIT;
        if (width != 0 && stencilDimensions.width != width) {
          sizeMatch = false;
        }
        if (height != 0 && static_cast<int>(stencilDimensions.height) != height) {
          sizeMatch = false;
        }
        width = static_cast<int>(stencilDimensions.width);
        height = static_cast<int>(stencilDimensions.height);
      }
      IGL_DEBUG_ASSERT(mask != 0);

      if (sizeMatch) {
        igl::opengl::PlatformDevice::blitFramebuffer(framebuffer_,
                                                     0,
                                                     0,
                                                     width,
                                                     height,
                                                     resolveFramebuffer_,
                                                     0,
                                                     0,
                                                     width,
                                                     height,
                                                     mask,
                                                     getContext(),
                                                     &outResult);
      } else {
        IGL_DEBUG_ASSERT_NOT_REACHED();
      }
    }
  }
}

void RenderCommandEncoder::pushDebugGroupLabel(const char* label,
                                               const igl::Color& /*color*/) const {
  IGL_DEBUG_ASSERT(adapter_);
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    getContext().pushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
  } else {
    IGL_LOG_ERROR_ONCE(
        "RenderCommandEncoder::pushDebugGroupLabel not supported in this context!\n");
  }
}

void RenderCommandEncoder::insertDebugEventLabel(const char* label,
                                                 const igl::Color& /*color*/) const {
  IGL_DEBUG_ASSERT(adapter_);
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    getContext().debugMessageInsert(
        GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_LOW, -1, label);
  } else {
    IGL_LOG_ERROR_ONCE(
        "RenderCommandEncoder::insertDebugEventLabel not supported in this context!\n");
  }
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  IGL_DEBUG_ASSERT(adapter_);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    getContext().popDebugGroup();
  } else {
    IGL_LOG_ERROR_ONCE("RenderCommandEncoder::popDebugGroupLabel not supported in this context!\n");
  }
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setViewport(viewport);
  }
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setScissorRect(rect);
  }
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setPipelineState(pipelineState);
  }
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& depthStencilState) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setDepthStencilState(depthStencilState);
  }
}

void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc, const void* data) {
  IGL_DEBUG_ASSERT(uniformDesc.location >= 0,
                   "Invalid location passed to bindUniformBuffer: %d",
                   uniformDesc.location);
  IGL_DEBUG_ASSERT(data != nullptr, "Data cannot be null");
  if (IGL_DEBUG_VERIFY(adapter_) && data) {
    adapter_->setUniform(uniformDesc, data);
  }
}

void RenderCommandEncoder::bindBuffer(uint32_t index,
                                      IBuffer* buffer,
                                      size_t offset,
                                      size_t bufferSize) {
  if (IGL_DEBUG_VERIFY(adapter_) && buffer) {
    auto* glBuffer = static_cast<Buffer*>(buffer);
    auto bufferType = glBuffer->getType();

    if (bufferType == Buffer::Type::Uniform) {
      IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    } else if (bufferType == Buffer::Type::UniformBlock) {
      adapter_->setUniformBuffer(glBuffer, offset, bufferSize, index);
    }
  }
}

void RenderCommandEncoder::bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    Buffer& glBuffer = static_cast<Buffer&>(buffer);

    IGL_DEBUG_ASSERT(glBuffer.getType() == Buffer::Type::Attribute);

    adapter_->setVertexBuffer(glBuffer, bufferOffset, static_cast<int>(index));
  }
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    indexType_ = toGlType(format);
    indexBufferOffset_ = reinterpret_cast<void*>(bufferOffset);
    adapter_->setIndexBuffer((Buffer&)buffer);
  }
}

void RenderCommandEncoder::bindBytes(size_t /*index*/,
                                     uint8_t /*target*/,
                                     const void* /*data*/,
                                     size_t /*length*/) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::bindPushConstants(const void* /*data*/,
                                             size_t /*length*/,
                                             size_t /*offset*/) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t bindTarget,
                                            ISamplerState* samplerState) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  if (IGL_DEBUG_VERIFY(adapter_)) {
    if ((bindTarget & BindTarget::kVertex) != 0) {
      adapter_->setVertexSamplerState(samplerState, index);
    }
    if ((bindTarget & BindTarget::kFragment) != 0) {
      adapter_->setFragmentSamplerState(samplerState, index);
    }
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void RenderCommandEncoder::bindTexture(size_t index, uint8_t bindTarget, ITexture* texture) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    if ((bindTarget & BindTarget::kVertex) != 0) {
      adapter_->setVertexTexture(texture, index);
    }
    if ((bindTarget & BindTarget::kFragment) != 0) {
      adapter_->setFragmentTexture(texture, index);
    }
  }
}

void RenderCommandEncoder::bindTexture(size_t index, ITexture* texture) {
  bindTexture(index, igl::BindTarget::kFragment, texture);
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  (void)baseInstance;

  IGL_DEBUG_ASSERT(baseInstance == 0, "Instancing is not implemented");

  if (IGL_DEBUG_VERIFY(adapter_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    auto mode = toGlPrimitive(adapter_->pipelineState().getRenderPipelineDesc().topology);
    if (instanceCount > 1) {
      adapter_->drawArraysInstanced(
          mode, (GLsizei)firstVertex, (GLsizei)vertexCount, (GLsizei)instanceCount);
    } else {
      adapter_->drawArrays(mode, (GLsizei)firstVertex, (GLsizei)vertexCount);
    }
  }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  (void)vertexOffset;
  (void)baseInstance;

  IGL_DEBUG_ASSERT(vertexOffset == 0, "vertexOffset is not implemented");
  IGL_DEBUG_ASSERT(baseInstance == 0, "Instancing is not implemented");
  IGL_DEBUG_ASSERT(indexType_, "No index buffer bound");

  const size_t indexOffsetBytes = static_cast<size_t>(firstIndex) * getIndexByteSize(indexType_);

  if (IGL_DEBUG_VERIFY(adapter_ && indexType_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    auto mode = toGlPrimitive(adapter_->pipelineState().getRenderPipelineDesc().topology);
    if (instanceCount > 1) {
      adapter_->drawElementsInstanced(mode,
                                      (GLsizei)indexCount,
                                      indexType_,
                                      (uint8_t*)indexBufferOffset_ + indexOffsetBytes,
                                      instanceCount);
    } else {
      adapter_->drawElements(
          mode, (GLsizei)indexCount, indexType_, (uint8_t*)indexBufferOffset_ + indexOffsetBytes);
    }
  }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void RenderCommandEncoder::multiDrawIndirect(IBuffer& indirectBuffer,
                                             size_t indirectBufferOffset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  if (IGL_DEBUG_VERIFY(adapter_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    const auto mode = toGlPrimitive(adapter_->pipelineState().getRenderPipelineDesc().topology);
    const auto* indirectBufferOffsetPtr = reinterpret_cast<uint8_t*>(indirectBufferOffset);
    for (uint32_t i = 0; i != drawCount; i++) {
      adapter_->drawArraysIndirect(mode, (Buffer&)indirectBuffer, indirectBufferOffsetPtr);
      indirectBufferOffsetPtr += stride ? stride : 16u; // sizeof(DrawArraysIndirectCommand)
    }
  }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  IGL_DEBUG_ASSERT(indexType_, "No index buffer bound");

  // TODO: use glMultiDrawElementsIndirect() when available

  if (IGL_DEBUG_VERIFY(adapter_ && indexType_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    const auto mode = toGlPrimitive(adapter_->pipelineState().getRenderPipelineDesc().topology);
    const auto* indirectBufferOffsetPtr = reinterpret_cast<uint8_t*>(indirectBufferOffset);
    for (uint32_t i = 0; i != drawCount; i++) {
      adapter_->drawElementsIndirect(
          mode, indexType_, (Buffer&)indirectBuffer, indirectBufferOffsetPtr);
      indirectBufferOffsetPtr += stride ? stride : 20u; // sizeof(DrawElementsIndirectCommand)
    }
  }
}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setStencilReferenceValue(value);
  }
}

void RenderCommandEncoder::setBlendColor(const Color& color) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setBlendColor(color);
  }
}

void RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp) {
  if (IGL_DEBUG_VERIFY(adapter_)) {
    adapter_->setDepthBias(depthBias, slopeScale, clamp);
  }
}

void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle handle) {
  if (handle.empty()) {
    return;
  }

  const BindGroupTextureDesc* desc = getContext().bindGroupTexturesPool_.get(handle);

  for (uint32_t i = 0; i != IGL_TEXTURE_SAMPLERS_MAX; i++) {
    if (desc->textures[i]) {
      IGL_DEBUG_ASSERT(desc->samplers[i]);
      bindTexture(i, BindTarget::kAllGraphics, desc->textures[i].get());
      bindSamplerState(i, BindTarget::kAllGraphics, desc->samplers[i].get());
    }
  }
}

void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle handle,
                                         uint32_t numDynamicOffsets,
                                         const uint32_t* dynamicOffsets) {
  if (handle.empty()) {
    return;
  }

  const BindGroupBufferDesc* desc = getContext().bindGroupBuffersPool_.get(handle);

  uint32_t dynamicOffset = 0;

  for (uint32_t i = 0; i != IGL_UNIFORM_BLOCKS_BINDING_MAX; i++) {
    if (desc->buffers[i]) {
      if (desc->isDynamicBufferMask & (1 << i)) {
        IGL_DEBUG_ASSERT(dynamicOffsets, "No dynamic offsets provided");
        IGL_DEBUG_ASSERT(dynamicOffset < numDynamicOffsets, "Not enough dynamic offsets provided");
        bindBuffer(i,
                   desc->buffers[i].get(),
                   desc->offset[i] + dynamicOffsets[dynamicOffset++],
                   desc->size[i]);
      } else {
        bindBuffer(i, desc->buffers[i].get(), desc->offset[i], desc->size[i]);
      }
    }
  }

  IGL_DEBUG_ASSERT(dynamicOffset == numDynamicOffsets, "Not all dynamic offsets were consumed");
}

} // namespace igl::opengl
