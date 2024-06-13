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
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderCommandAdapter.h>
#include <igl/opengl/RenderPipelineState.h>
#include <igl/opengl/SamplerState.h>
#include <igl/opengl/Shader.h>
#include <igl/opengl/Texture.h>
#include <igl/opengl/UniformAdapter.h>
#include <igl/opengl/VertexInputState.h>

namespace igl {
namespace opengl {

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

int toGlType(IndexFormat format) {
  switch (format) {
  case IndexFormat::UInt16:
    return GL_UNSIGNED_SHORT;
  case IndexFormat::UInt32:
    return GL_UNSIGNED_INT;
  }
  IGL_UNREACHABLE_RETURN(GL_UNSIGNED_INT)
}

} // namespace

RenderCommandEncoder::RenderCommandEncoder(std::shared_ptr<CommandBuffer> commandBuffer) :
  IRenderCommandEncoder(std::move(commandBuffer)),
  WithContext(static_cast<CommandBuffer&>(getCommandBuffer()).getContext()) {}

std::unique_ptr<RenderCommandEncoder> RenderCommandEncoder::create(
    std::shared_ptr<CommandBuffer> commandBuffer,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& dependencies,
    Result* outResult) {
  if (!commandBuffer) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "commandBuffer was null");
    return {};
  }

  std::unique_ptr<RenderCommandEncoder> newEncoder(
      new RenderCommandEncoder(std::move(commandBuffer)));
  newEncoder->beginEncoding(renderPass, framebuffer, outResult);
  return newEncoder;
}

RenderCommandEncoder::~RenderCommandEncoder() = default;

void RenderCommandEncoder::beginEncoding(const RenderPassDesc& renderPass,
                                         const std::shared_ptr<IFramebuffer>& framebuffer,
                                         Result* outResult) {
  // Save caller state
  auto& context = getContext();

  scissorEnabled_ = context.isEnabled(GL_SCISSOR_TEST);
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
  framebuffer_ = std::static_pointer_cast<igl::opengl::Framebuffer>(framebuffer);
  resolveFramebuffer_ = framebuffer_->getResolveFramebuffer();
  Result::setOk(outResult);
}

void RenderCommandEncoder::endEncoding() {
  if (IGL_VERIFY(adapter_)) {
    // Restore caller state
    getContext().setEnabled(scissorEnabled_, GL_SCISSOR_TEST);

    // Disable depthBias
    getContext().setEnabled(false, GL_POLYGON_OFFSET_FILL);
    adapter_->setDepthBias(0.0f, 0.0f);

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
      IGL_ASSERT(mask != 0);

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
        IGL_ASSERT_NOT_REACHED();
      }
    }
  }
}

void RenderCommandEncoder::pushDebugGroupLabel(const char* label,
                                               const igl::Color& /*color*/) const {
  IGL_ASSERT(adapter_);
  IGL_ASSERT(label != nullptr && *label);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    std::string_view labelSV(label);
    getContext().pushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, labelSV.length(), labelSV.data());
  } else {
    IGL_LOG_ERROR_ONCE(
        "RenderCommandEncoder::pushDebugGroupLabel not supported in this context!\n");
  }
}

void RenderCommandEncoder::insertDebugEventLabel(const char* label,
                                                 const igl::Color& /*color*/) const {
  IGL_ASSERT(adapter_);
  IGL_ASSERT(label != nullptr && *label);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    std::string_view labelSV(label);
    getContext().debugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                                    GL_DEBUG_TYPE_MARKER,
                                    0,
                                    GL_DEBUG_SEVERITY_LOW,
                                    labelSV.length(),
                                    labelSV.data());
  } else {
    IGL_LOG_ERROR_ONCE(
        "RenderCommandEncoder::insertDebugEventLabel not supported in this context!\n");
  }
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  IGL_ASSERT(adapter_);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    getContext().popDebugGroup();
  } else {
    IGL_LOG_ERROR_ONCE("RenderCommandEncoder::popDebugGroupLabel not supported in this context!\n");
  }
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setViewport(viewport);
  }
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setScissorRect(rect);
  }
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setPipelineState(pipelineState);
  }
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& depthStencilState) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setDepthStencilState(depthStencilState);
  }
}

void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc, const void* data) {
  IGL_ASSERT_MSG(uniformDesc.location >= 0,
                 "Invalid location passed to bindUniformBuffer: %d",
                 uniformDesc.location);
  IGL_ASSERT_MSG(data != nullptr, "Data cannot be null");
  if (IGL_VERIFY(adapter_) && data) {
    adapter_->setUniform(uniformDesc, data);
  }
}

void RenderCommandEncoder::bindBuffer(int index,
                                      const std::shared_ptr<IBuffer>& buffer,
                                      size_t offset,
                                      size_t bufferSize) {
  (void)bufferSize;

  IGL_ASSERT_MSG(index >= 0, "Invalid index passed to bindBuffer: %d", index);

  if (IGL_VERIFY(adapter_) && buffer) {
    auto glBuffer = std::static_pointer_cast<Buffer>(buffer);
    auto bufferType = glBuffer->getType();

    if (bufferType == Buffer::Type::Uniform) {
      IGL_ASSERT_NOT_IMPLEMENTED();
    } else if (bufferType == Buffer::Type::UniformBlock) {
      adapter_->setUniformBuffer(glBuffer, offset, index);
    }
  }
}

void RenderCommandEncoder::bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset) {
  if (IGL_VERIFY(adapter_)) {
    Buffer& glBuffer = static_cast<Buffer&>(buffer);

    IGL_ASSERT(glBuffer.getType() == Buffer::Type::Attribute);

    adapter_->setVertexBuffer(glBuffer, bufferOffset, static_cast<int>(index));
  }
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  if (IGL_VERIFY(adapter_)) {
    indexType_ = toGlType(format);
    indexBufferOffset_ = reinterpret_cast<void*>(bufferOffset);
    adapter_->setIndexBuffer((Buffer&)buffer);
  }
}

void RenderCommandEncoder::bindBytes(size_t /*index*/,
                                     uint8_t /*target*/,
                                     const void* /*data*/,
                                     size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::bindPushConstants(const void* /*data*/,
                                             size_t /*length*/,
                                             size_t /*offset*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t bindTarget,
                                            ISamplerState* samplerState) {
  if (IGL_VERIFY(adapter_)) {
    if ((bindTarget & BindTarget::kVertex) != 0) {
      adapter_->setVertexSamplerState(samplerState, index);
    }
    if ((bindTarget & BindTarget::kFragment) != 0) {
      adapter_->setFragmentSamplerState(samplerState, index);
    }
  }
}

void RenderCommandEncoder::bindTexture(size_t index, uint8_t bindTarget, ITexture* texture) {
  if (IGL_VERIFY(adapter_)) {
    if ((bindTarget & BindTarget::kVertex) != 0) {
      adapter_->setVertexTexture(texture, index);
    }
    if ((bindTarget & BindTarget::kFragment) != 0) {
      adapter_->setFragmentTexture(texture, index);
    }
  }
}

void RenderCommandEncoder::draw(PrimitiveType primitiveType,
                                size_t vertexStart,
                                size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t baseInstance) {
  (void)instanceCount;
  (void)baseInstance;

  IGL_ASSERT_MSG(instanceCount == 1, "Instancing is not implemented");
  IGL_ASSERT_MSG(baseInstance == 0, "Instancing is not implemented");

  if (IGL_VERIFY(adapter_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    auto mode = toGlPrimitive(primitiveType);
    adapter_->drawArrays(mode, (GLsizei)vertexStart, (GLsizei)vertexCount);
  }
}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  (void)instanceCount;
  (void)baseInstance;

  IGL_ASSERT_MSG(instanceCount == 1, "Instancing is not implemented");
  IGL_ASSERT_MSG(baseInstance == 0, "Instancing is not implemented");

  if (IGL_VERIFY(adapter_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    auto mode = toGlPrimitive(adapter_->pipelineState().getRenderPipelineDesc().topology);
    adapter_->drawArrays(mode, (GLsizei)firstVertex, (GLsizei)vertexCount);
  }
}

void RenderCommandEncoder::drawIndexed(PrimitiveType primitiveType,
                                       size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  (void)instanceCount;
  (void)vertexOffset;
  (void)baseInstance;

  IGL_ASSERT_MSG(vertexOffset == 0, "vertexOffset is not implemented");
  IGL_ASSERT_MSG(baseInstance == 0, "Instancing is not implemented");
  IGL_ASSERT_MSG(indexType_, "No index buffer bound");

  const size_t indexOffsetBytes =
      static_cast<size_t>(firstIndex) * (indexType_ == GL_UNSIGNED_INT ? 4u : 2u);

  if (IGL_VERIFY(adapter_ && indexType_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    auto mode = toGlPrimitive(primitiveType);
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

void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  (void)instanceCount;
  (void)vertexOffset;
  (void)baseInstance;

  IGL_ASSERT_MSG(instanceCount == 1, "Instancing is not implemented");
  IGL_ASSERT_MSG(vertexOffset == 0, "vertexOffset is not implemented");
  IGL_ASSERT_MSG(baseInstance == 0, "Instancing is not implemented");
  IGL_ASSERT_MSG(indexType_, "No index buffer bound");

  const size_t indexOffsetBytes =
      static_cast<size_t>(firstIndex) * (indexType_ == GL_UNSIGNED_INT ? 4u : 2u);

  if (IGL_VERIFY(adapter_ && indexType_)) {
    getCommandBuffer().incrementCurrentDrawCount();
    auto mode = toGlPrimitive(adapter_->pipelineState().getRenderPipelineDesc().topology);
    adapter_->drawElements(
        mode, (GLsizei)indexCount, indexType_, (uint8_t*)indexBufferOffset_ + indexOffsetBytes);
  }
}

void RenderCommandEncoder::multiDrawIndirect(IBuffer& /*indirectBuffer*/,
                                             size_t /*indirectBufferOffset*/,
                                             uint32_t /*drawCount*/,
                                             uint32_t /*stride*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  IGL_ASSERT_MSG(indexType_, "No index buffer bound");

  // TODO: use glMultiDrawElementsIndirect() when available

  if (IGL_VERIFY(adapter_ && indexType_)) {
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
  if (IGL_VERIFY(adapter_)) {
    adapter_->setStencilReferenceValue(value);
  }
}

void RenderCommandEncoder::setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setStencilReferenceValues(frontValue, backValue);
  }
}

void RenderCommandEncoder::setBlendColor(Color color) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setBlendColor(color);
  }
}

void RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float /*clamp*/) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setDepthBias(depthBias, slopeScale);
  }
}

} // namespace opengl
} // namespace igl
