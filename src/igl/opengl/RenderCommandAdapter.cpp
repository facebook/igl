/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Common.h>
#include <igl/opengl/RenderCommandAdapter.h>

#include <algorithm>
#include <igl/RenderCommandEncoder.h>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/DepthStencilState.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderPipelineState.h>
#include <igl/opengl/SamplerState.h>
#include <igl/opengl/Shader.h>
#include <igl/opengl/Texture.h>
#include <igl/opengl/UniformAdapter.h>
#include <igl/opengl/VertexArrayObject.h>

#define SET_DIRTY(dirtyMap, index) dirtyMap.set(index)
#define CLEAR_DIRTY(dirtyMap, index) dirtyMap.reset(index)
#define IS_DIRTY(dirtyMap, index) dirtyMap[index]

namespace igl::opengl {
RenderCommandAdapter::RenderCommandAdapter(IContext& context) :
  WithContext(context),
  uniformAdapter_(UniformAdapter(context, UniformAdapter::PipelineType::Render)) {
  useVAO_ = context.deviceFeatures().hasInternalFeature(InternalFeatures::VertexArrayObject);
  if (useVAO_) {
    activeVAO_ = std::make_shared<VertexArrayObject>(getContext());
    activeVAO_->create();
  }
}

std::unique_ptr<RenderCommandAdapter> RenderCommandAdapter::create(
    IContext& context,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    Result* outResult) {
  std::unique_ptr<RenderCommandAdapter> newAdapter(new RenderCommandAdapter(context));
  newAdapter->initialize(renderPass, framebuffer, outResult);
  return newAdapter;
}

void RenderCommandAdapter::initialize(const RenderPassDesc& renderPass,
                                      const std::shared_ptr<IFramebuffer>& framebuffer,
                                      Result* outResult) {
  if (!IGL_DEBUG_VERIFY(framebuffer)) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "framebuffer is null");
    return;
  }
  if (activeVAO_) {
    if (!IGL_DEBUG_VERIFY(activeVAO_->isValid())) {
      Result::setResult(outResult, Result::Code::RuntimeError, "Vertex array object is invalid");
      return;
    }
    activeVAO_->bind();
  }
  const auto& openglFramebuffer = static_cast<const Framebuffer&>(*framebuffer);
  openglFramebuffer.bind(renderPass);

  auto viewport = openglFramebuffer.getViewport();
  IGL_DEBUG_ASSERT(!(viewport.width < 0.f) && !(viewport.height < 0.f));
  setViewport(viewport);
  Result::setOk(outResult);
}

void RenderCommandAdapter::setViewport(const Viewport& viewport) {
  getContext().viewport(
      (GLint)viewport.x, (GLint)viewport.y, (GLint)viewport.width, (GLint)viewport.height);
}

void RenderCommandAdapter::setScissorRect(const ScissorRect& rect) {
  const bool scissorEnabled = !rect.isNull();
  getContext().setEnabled(scissorEnabled, GL_SCISSOR_TEST);
  if (scissorEnabled) {
    getContext().scissor(rect.x, rect.y, rect.width, rect.height);
  }
}

void RenderCommandAdapter::setDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& newValue) {
  depthStencilState_ = newValue;
  setDirty(StateMask::DepthStencil);
}

void RenderCommandAdapter::setStencilReferenceValue(uint32_t value) {
  frontStencilReferenceValue_ = value;
  backStencilReferenceValue_ = value;

  setDirty(StateMask::DepthStencil);
}

void RenderCommandAdapter::setBlendColor(const Color& color) {
  getContext().blendColor(color.r, color.g, color.b, color.a);
}

void RenderCommandAdapter::setDepthBias(float depthBias, float slopeScale, float clamp) {
  getContext().setEnabled(true, GL_POLYGON_OFFSET_FILL);
  getContext().polygonOffsetClamp(slopeScale, depthBias, clamp);
}

void RenderCommandAdapter::clearVertexBuffers() {
  vertexBuffersDirty_.reset();
}

void RenderCommandAdapter::setVertexBuffer(Buffer& buffer,
                                           size_t offset,
                                           size_t index,
                                           Result* outResult) {
  IGL_DEBUG_ASSERT(index < IGL_BUFFER_BINDINGS_MAX,
                   "Buffer index is beyond max, may want to increase limit");
  if (index < IGL_BUFFER_BINDINGS_MAX) {
    vertexBuffers_[index] = {&buffer, offset};
    SET_DIRTY(vertexBuffersDirty_, index);
    Result::setOk(outResult);
  } else {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
  }
}

void RenderCommandAdapter::setIndexBuffer(Buffer& buffer) {
  bindBufferWithShaderStorageBufferOverride(buffer, GL_ELEMENT_ARRAY_BUFFER);
}

void RenderCommandAdapter::clearUniformBuffers() {
  uniformAdapter_.clearUniformBuffers();
}

void RenderCommandAdapter::setUniform(const UniformDesc& uniformDesc,
                                      const void* data,
                                      Result* outResult) {
  uniformAdapter_.setUniform(uniformDesc, data, outResult);
}

void RenderCommandAdapter::setUniformBuffer(Buffer* buffer,
                                            size_t offset,
                                            size_t size,
                                            uint32_t index,
                                            Result* outResult) {
  uniformAdapter_.setUniformBuffer(buffer, offset, size, index, outResult);
}

void RenderCommandAdapter::clearVertexTexture() {
  vertexTextureStates_ = TextureStates();
  vertexTextureStatesDirty_.reset();
}

void RenderCommandAdapter::setVertexTexture(ITexture* texture, size_t index, Result* outResult) {
  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  if (vertexTextureStates_[index].first != texture) {
    vertexTextureStates_[index].first = texture;
    SET_DIRTY(vertexTextureStatesDirty_, index);
  }
  Result::setOk(outResult);
}

void RenderCommandAdapter::setVertexSamplerState(ISamplerState* samplerState,
                                                 size_t index,
                                                 Result* outResult) {
  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  if (vertexTextureStates_[index].second != samplerState) {
    vertexTextureStates_[index].second = samplerState;
    SET_DIRTY(vertexTextureStatesDirty_, index);
  }
  Result::setOk(outResult);
}

void RenderCommandAdapter::clearFragmentTexture() {
  fragmentTextureStates_ = TextureStates();
  fragmentTextureStatesDirty_.reset();
}

void RenderCommandAdapter::setFragmentTexture(ITexture* texture, size_t index, Result* outResult) {
  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  if (fragmentTextureStates_[index].first != texture) {
    fragmentTextureStates_[index].first = texture;
    SET_DIRTY(fragmentTextureStatesDirty_, index);
  }
  Result::setOk(outResult);
}

void RenderCommandAdapter::setFragmentSamplerState(ISamplerState* samplerState,
                                                   size_t index,
                                                   Result* outResult) {
  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  if (fragmentTextureStates_[index].second != samplerState) {
    fragmentTextureStates_[index].second = samplerState;
    SET_DIRTY(fragmentTextureStatesDirty_, index);
  }
  Result::setOk(outResult);
}

// When pipelineState is modified, all dependent resources are cleared
void RenderCommandAdapter::clearDependentResources(
    const std::shared_ptr<IRenderPipelineState>& newValue,
    Result* outResult) {
  auto* curStateOpenGL = static_cast<RenderPipelineState*>(pipelineState_.get());
  if (!IGL_DEBUG_VERIFY(curStateOpenGL)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "pipeline state is null");
    return;
  }

  auto* newStateOpenGL = static_cast<RenderPipelineState*>(newValue.get());

  if (!newStateOpenGL || !curStateOpenGL->matchesShaderProgram(*newStateOpenGL)) {
    // Don't use previously set resources. Uniforms/texture locations not same between programs
    uniformAdapter_.clearUniformBuffers();
    clearVertexTexture();
    clearFragmentTexture();
  }

  if (curStateOpenGL && newStateOpenGL) {
    newStateOpenGL->savePrevPipelineStateAttributesLocations(*curStateOpenGL);
  }

  if (!newStateOpenGL || !curStateOpenGL->matchesVertexInputState(*newStateOpenGL)) {
    // Don't reuse previously set vertex buffers.
    clearVertexBuffers();
  }
  Result::setOk(outResult);
}

void RenderCommandAdapter::setPipelineState(const std::shared_ptr<IRenderPipelineState>& newValue,
                                            Result* outResult) {
  Result::setOk(outResult);
  if (pipelineState_) {
    clearDependentResources(newValue, outResult); // Only clear if pipeline state was previously set
  }
  pipelineState_ = newValue;
  setDirty(StateMask::PIPELINE);
}

void RenderCommandAdapter::drawArrays(GLenum mode, GLint first, GLsizei count) {
  willDraw();
  getContext().drawArrays(toMockWireframeMode(mode), first, count);
  didDraw();
}

void RenderCommandAdapter::drawArraysIndirect(GLenum mode,
                                              Buffer& indirectBuffer,
                                              const GLvoid* indirectBufferOffset) {
  willDraw();
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DrawArraysIndirect)) {
    bindBufferWithShaderStorageBufferOverride(indirectBuffer, GL_DRAW_INDIRECT_BUFFER);
    getContext().drawArraysIndirect(toMockWireframeMode(mode), indirectBufferOffset);
  } else {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  }
  didDraw();
}

void RenderCommandAdapter::drawArraysInstanced(GLenum mode,
                                               GLint first,
                                               GLsizei count,
                                               GLsizei instancecount) {
  willDraw();
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::DrawInstanced)) {
    getContext().drawArraysInstanced(toMockWireframeMode(mode), first, count, instancecount);
  } else {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  }
  didDraw();
}

void RenderCommandAdapter::drawElements(GLenum mode,
                                        GLsizei indexCount,
                                        GLenum indexType,
                                        const GLvoid* indexOffset) {
  willDraw();
  getContext().drawElements(toMockWireframeMode(mode), indexCount, indexType, indexOffset);
  didDraw();
}

void RenderCommandAdapter::drawElementsInstanced(GLenum mode,
                                                 GLsizei indexCount,
                                                 GLenum indexType,
                                                 const GLvoid* indexOffset,
                                                 GLsizei instancecount) {
  willDraw();
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::DrawInstanced)) {
    getContext().drawElementsInstanced(
        toMockWireframeMode(mode), indexCount, indexType, indexOffset, instancecount);
  } else {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  }
  didDraw();
}

void RenderCommandAdapter::drawElementsIndirect(GLenum mode,
                                                GLenum indexType,
                                                Buffer& indirectBuffer,
                                                const GLvoid* indirectBufferOffset) {
  willDraw();
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::DrawIndexedIndirect)) {
    bindBufferWithShaderStorageBufferOverride(indirectBuffer, GL_DRAW_INDIRECT_BUFFER);
    getContext().drawElementsIndirect(toMockWireframeMode(mode), indexType, indirectBufferOffset);
  } else {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  }
  didDraw();
}

void RenderCommandAdapter::endEncoding() {
  // Some minimal cleanup needs to occur in order. Otherwise, OpenGL can end in a bad state
  // with complex rendering.
  if (pipelineState_) {
    unbindVertexAttributes();
  }

  pipelineState_ = nullptr;
  depthStencilState_ = nullptr;

  uniformAdapter_.shrinkUniformUsage();
  uniformAdapter_.clearUniformBuffers();
  vertexTextureStates_ = TextureStates();
  fragmentTextureStates_ = TextureStates();

  vertexBuffersDirty_.reset();
  vertexTextureStatesDirty_.reset();
  fragmentTextureStatesDirty_.reset();
  dirtyStateBits_ = EnumToValue(StateMask::NONE);
}

void RenderCommandAdapter::willDraw() {
  Result ret;
  auto* pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());

  // Vertex Buffers must be bound before pipelineState->bind()
  if (pipelineState) {
    pipelineState->clearActiveAttributesLocations();
    for (size_t bufferIndex = 0; bufferIndex < IGL_BUFFER_BINDINGS_MAX; ++bufferIndex) {
      if (IS_DIRTY(vertexBuffersDirty_, bufferIndex)) {
        auto& bufferState = vertexBuffers_[bufferIndex];
        bindBufferWithShaderStorageBufferOverride((*bufferState.resource), GL_ARRAY_BUFFER);
        // now bind the vertex attributes corresponding to this vertex buffer
        pipelineState->bindVertexAttributes(bufferIndex, bufferState.offset);
        CLEAR_DIRTY(vertexBuffersDirty_, bufferIndex);
      }
    }
    pipelineState->unbindPrevPipelineVertexAttributes();
    if (isDirty(StateMask::PIPELINE)) {
      pipelineState->bind();
      clearDirty(StateMask::PIPELINE);
    }
  }

  auto* depthStencilState = static_cast<DepthStencilState*>(depthStencilState_.get());
  if (depthStencilState && isDirty(StateMask::DepthStencil)) {
    depthStencilState->bind(frontStencilReferenceValue_, backStencilReferenceValue_);
    clearDirty(StateMask::DepthStencil);
  }

  // We store 2 parallel vectors (one for uniforms and one for uniform blocks)
  // that behave as a queue of uniforms waiting to be issued to the GPU.
  // These were set between the previous draw call and the current draw call.
  // At the end of the draw call, the queue can be treated as empty since we
  // don't need to re-issue them (they'll carry over to the next draw call unless
  // a subsequent uniform at the same location is bound).
  //
  // Various error-checking does _not_ happen in production, since this is the "inner loop":
  //
  // * Duplicate uniform and uniform block at same location (one in each vector)
  // * Duplicate uniforms in the vector with the same location (same for uniform blocks)
  //
  // These should be considered client bugs, so an assert fires in local dev builds.

  // this is actually compile time defined and doesn't change, cached these statically.
  static const size_t kVertexTextureStatesSize = vertexTextureStates_.size();
  static const size_t kFragmentTextureStatesSize = fragmentTextureStates_.size();
  if (pipelineState) {
    // Bind uniforms to be used for render
    uniformAdapter_.bindToPipeline(getContext());
    for (size_t index = 0; index < kVertexTextureStatesSize; index++) {
      if (!IS_DIRTY(vertexTextureStatesDirty_, index)) {
        continue;
      }
      auto& textureState = vertexTextureStates_[index];
      if (auto* texture = static_cast<Texture*>(textureState.first)) {
        ret = pipelineState->bindTextureUnit(index, igl::BindTarget::kVertex);

        if (!ret.isOk()) {
          IGL_LOG_INFO_ONCE(ret.message.c_str());
          continue;
        }

        texture->bind();

        if (auto* samplerState = static_cast<SamplerState*>(textureState.second)) {
          samplerState->bind(texture);
        }
        CLEAR_DIRTY(vertexTextureStatesDirty_, index);
      }
    }
    for (size_t index = 0; index < kFragmentTextureStatesSize; index++) {
      if (!IS_DIRTY(fragmentTextureStatesDirty_, index)) {
        continue;
      }
      auto& textureState = fragmentTextureStates_[index];
      if (auto* texture = static_cast<Texture*>(textureState.first)) {
        ret = pipelineState->bindTextureUnit(index, igl::BindTarget::kFragment);

        if (!ret.isOk()) {
          IGL_LOG_INFO_ONCE(ret.message.c_str());
          continue;
        }
        texture->bind();

        if (auto* samplerState = static_cast<SamplerState*>(textureState.second)) {
          samplerState->bind(texture);
        }
        CLEAR_DIRTY(fragmentTextureStatesDirty_, index);
      }
    }

    if (getContext().shouldValidateShaders()) {
      const auto* stages = pipelineState->getShaderStages();
      if (stages) {
        const auto result = stages->validate();
        IGL_DEBUG_ASSERT(result.isOk(), result.message.c_str());
      }
    }
  }
}

GLenum RenderCommandAdapter::toMockWireframeMode(GLenum mode) const {
#if defined(IGL_OPENGL_ES)
  auto* const pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());
  const bool modeNeedsConversion = mode == GL_TRIANGLES || mode != GL_TRIANGLE_STRIP;
  if (pipelineState->getPolygonFillMode() == igl::PolygonFillMode::Line && modeNeedsConversion) {
    return GL_LINE_STRIP;
  }
#endif

  return mode;
}

void RenderCommandAdapter::didDraw() {
  // Placeholder stub in case we want to add something later
}

void RenderCommandAdapter::unbindVertexAttributes() {
  auto* pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());
  if (pipelineState) {
    pipelineState->unbindVertexAttributes();
  }
}

void RenderCommandAdapter::bindBufferWithShaderStorageBufferOverride(
    Buffer& buffer,
    GLenum overrideTargetForShaderStorageBuffer) {
  auto& arrayBuffer = static_cast<ArrayBuffer&>(buffer);
  if (arrayBuffer.getTarget() == GL_SHADER_STORAGE_BUFFER) {
    arrayBuffer.bindForTarget(overrideTargetForShaderStorageBuffer);
  } else {
    arrayBuffer.bind();
  }
}
} // namespace igl::opengl
