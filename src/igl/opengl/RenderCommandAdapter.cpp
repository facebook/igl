/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Common.h>
#include <igl/opengl/RenderCommandAdapter.h>

#include <algorithm>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/DepthStencilState.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <igl/opengl/RenderPipelineState.h>
#include <igl/opengl/SamplerState.h>
#include <igl/opengl/Shader.h>
#include <igl/opengl/Texture.h>
#include <igl/opengl/VertexArrayObject.h>
#include <igl/opengl/VertexInputState.h>

#define SET_DIRTY(dirtyMap, index) dirtyMap.set(index)
#define CLEAR_DIRTY(dirtyMap, index) dirtyMap.reset(index)
#define IS_DIRTY(dirtyMap, index) dirtyMap[index]

namespace igl {
namespace opengl {
RenderCommandAdapter::RenderCommandAdapter(IContext& context) :
  WithContext(context),
  uniformAdapter_(UniformAdapter(context, UniformAdapter::PipelineType::Render)),
  cachedUnbindPolicy_(getContext().getUnbindPolicy()) {
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
  cachedUnbindPolicy_ = getContext().getUnbindPolicy();

  if (!IGL_VERIFY(framebuffer)) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "framebuffer is null");
    return;
  }
  if (activeVAO_) {
    activeVAO_->bind();
  }
  const auto& openglFramebuffer = static_cast<const Framebuffer&>(*framebuffer);
  openglFramebuffer.bind(renderPass);

  auto viewport = openglFramebuffer.getViewport();
  IGL_ASSERT(!(viewport.width < 0.f) && !(viewport.height < 0.f));
  setViewport(viewport);
  Result::setOk(outResult);
}

void RenderCommandAdapter::setViewport(const Viewport& viewport) {
  getContext().viewport(
      (GLint)viewport.x, (GLint)viewport.y, (GLint)viewport.width, (GLint)viewport.height);
}

void RenderCommandAdapter::setScissorRect(const ScissorRect& rect) {
  bool scissorEnabled = !rect.isNull();
  getContext().setEnabled(scissorEnabled, GL_SCISSOR_TEST);
  if (scissorEnabled) {
    getContext().scissor(rect.x, rect.y, rect.width, rect.height);
  }
}

void RenderCommandAdapter::setDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& newValue) {
  depthStencilState_ = newValue;
  setDirty(StateMask::DEPTH_STENCIL);
}

void RenderCommandAdapter::setStencilReferenceValue(uint32_t value, Result* outResult) {
  if (!IGL_VERIFY(depthStencilState_)) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "depth stencil state is null");
    return;
  }
  auto& depthStencilState = static_cast<DepthStencilState&>(*depthStencilState_);
  depthStencilState.setStencilReferenceValue(value);
  setDirty(StateMask::DEPTH_STENCIL);
  Result::setOk(outResult);
}

void RenderCommandAdapter::setStencilReferenceValues(uint32_t frontValue,
                                                     uint32_t backValue,
                                                     Result* outResult) {
  if (!IGL_VERIFY(depthStencilState_)) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "depth stencil state is null");
    return;
  }
  auto& depthStencilState = static_cast<DepthStencilState&>(*depthStencilState_);
  depthStencilState.setStencilReferenceValues(frontValue, backValue);
  setDirty(StateMask::DEPTH_STENCIL);
  Result::setOk(outResult);
}

void RenderCommandAdapter::setBlendColor(Color color) {
  getContext().blendColor(color.r, color.g, color.b, color.a);
}

void RenderCommandAdapter::setDepthBias(float depthBias, float slopeScale) {
  getContext().setEnabled(true, GL_POLYGON_OFFSET_FILL);
  getContext().polygonOffset(slopeScale, depthBias);
}

void RenderCommandAdapter::clearVertexBuffers() {
  vertexBuffersDirty_.reset();
}

void RenderCommandAdapter::setVertexBuffer(std::shared_ptr<Buffer> buffer,
                                           size_t offset,
                                           int index,
                                           Result* outResult) {
  IGL_ASSERT_MSG(index >= 0, "Invalid index passed to setVertexBuffer");
  IGL_ASSERT_MSG(index < IGL_VERTEX_BUFFER_MAX,
                 "Buffer index is beyond max, may want to increase limit");
  if (index >= 0 && index < IGL_VERTEX_BUFFER_MAX && buffer) {
    vertexBuffers_[index] = {std::move(buffer), offset};
    SET_DIRTY(vertexBuffersDirty_, index);
    Result::setOk(outResult);
  } else {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
  }
}

void RenderCommandAdapter::clearUniformBuffers() {
  uniformAdapter_.clearUniformBuffers();
}

void RenderCommandAdapter::setUniform(const UniformDesc& uniformDesc,
                                      const void* data,
                                      Result* outResult) {
  uniformAdapter_.setUniform(uniformDesc, data, outResult);
}

void RenderCommandAdapter::setUniformBuffer(const std::shared_ptr<Buffer>& buffer,
                                            size_t offset,
                                            int index,
                                            Result* outResult) {
  uniformAdapter_.setUniformBuffer(buffer, offset, index, outResult);
}

void RenderCommandAdapter::clearVertexTexture() {
  vertexTextureStates_ = TextureStates();
  vertexTextureStatesDirty_.reset();
}

void RenderCommandAdapter::setVertexTexture(ITexture* texture, size_t index, Result* outResult) {
  if (!IGL_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  vertexTextureStates_[index].first = texture;
  SET_DIRTY(vertexTextureStatesDirty_, index);
  Result::setOk(outResult);
}

void RenderCommandAdapter::setVertexSamplerState(const std::shared_ptr<ISamplerState>& samplerState,
                                                 size_t index,
                                                 Result* outResult) {
  if (!IGL_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  vertexTextureStates_[index].second = samplerState;
  SET_DIRTY(vertexTextureStatesDirty_, index);
  Result::setOk(outResult);
}

void RenderCommandAdapter::clearFragmentTexture() {
  fragmentTextureStates_ = TextureStates();
  fragmentTextureStatesDirty_.reset();
}

void RenderCommandAdapter::setFragmentTexture(ITexture* texture, size_t index, Result* outResult) {
  if (!IGL_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  fragmentTextureStates_[index].first = texture;
  SET_DIRTY(fragmentTextureStatesDirty_, index);
  Result::setOk(outResult);
}

void RenderCommandAdapter::setFragmentSamplerState(
    const std::shared_ptr<ISamplerState>& samplerState,
    size_t index,
    Result* outResult) {
  if (!IGL_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return;
  }
  fragmentTextureStates_[index].second = samplerState;
  SET_DIRTY(fragmentTextureStatesDirty_, index);
  Result::setOk(outResult);
}

// When pipelineState is modified, all dependent resources are cleared
void RenderCommandAdapter::clearDependentResources(
    const std::shared_ptr<IRenderPipelineState>& newValue,
    Result* outResult) {
  auto curStateOpenGL = static_cast<opengl::RenderPipelineState*>(pipelineState_.get());
  if (!IGL_VERIFY(curStateOpenGL)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "pipeline state is null");
    return;
  }

  auto newStateOpenGL = static_cast<opengl::RenderPipelineState*>(newValue.get());

  if (!newStateOpenGL || !curStateOpenGL->matchesShaderProgram(*newStateOpenGL)) {
    // Don't use previously set resources. Uniforms/texture locations not same between programs
    uniformAdapter_.clearUniformBuffers();
    clearVertexTexture();
    clearFragmentTexture();
  }

  if (!newStateOpenGL || !curStateOpenGL->matchesVertexInputState(*newStateOpenGL)) {
    // We do need to clear vertex attributes, when pipelinestate is modified.
    // If we don't, subsequent draw calls might try to read from these locations
    // and crashes might happen.
    unbindVertexAttributes();

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

void RenderCommandAdapter::drawElements(GLenum mode,
                                        GLsizei indexCount,
                                        GLenum indexType,
                                        Buffer& indexBuffer,
                                        const GLvoid* indexOffset) {
  willDraw();
  bindBufferWithShaderStorageBufferOverride(indexBuffer, GL_ELEMENT_ARRAY_BUFFER);
  getContext().drawElements(toMockWireframeMode(mode), indexCount, indexType, indexOffset);
  didDraw();
}

void RenderCommandAdapter::drawElementsIndirect(GLenum mode,
                                                GLenum indexType,
                                                Buffer& indexBuffer,
                                                Buffer& indirectBuffer,
                                                const GLvoid* indirectBufferOffset) {
  willDraw();
  bindBufferWithShaderStorageBufferOverride(indexBuffer, GL_ELEMENT_ARRAY_BUFFER);
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::DrawIndexedIndirect)) {
    bindBufferWithShaderStorageBufferOverride(indirectBuffer, GL_DRAW_INDIRECT_BUFFER);
    getContext().drawElementsIndirect(toMockWireframeMode(mode), indexType, indirectBufferOffset);
  } else {
    IGL_ASSERT_NOT_IMPLEMENTED();
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
  auto pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());

  // Vertex Buffers must be bound before pipelineState->bind()
  if (pipelineState) {
    for (size_t bufferIndex = 0; bufferIndex < IGL_VERTEX_BUFFER_MAX; ++bufferIndex) {
      if (IS_DIRTY(vertexBuffersDirty_, bufferIndex)) {
        auto& bufferState = vertexBuffers_[bufferIndex];
        bindBufferWithShaderStorageBufferOverride((*bufferState.resource), GL_ARRAY_BUFFER);
        // now bind the vertex attributes corresponding to this vertex buffer
        pipelineState->bindVertexAttributes(bufferIndex, bufferState.offset);
        CLEAR_DIRTY(vertexBuffersDirty_, bufferIndex);
      }
    }
    if (isDirty(StateMask::PIPELINE)) {
      pipelineState->bind();
      clearDirty(StateMask::PIPELINE);
    }
  }

  auto depthStencilState = static_cast<DepthStencilState*>(depthStencilState_.get());
  if (depthStencilState && isDirty(StateMask::DEPTH_STENCIL)) {
    depthStencilState->bind();
    clearDirty(StateMask::DEPTH_STENCIL);
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
  static size_t kVertexTextureStatesSize = vertexTextureStates_.size();
  static size_t kFragmentTextureStatesSize = fragmentTextureStates_.size();
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

        if (auto samplerState = static_cast<SamplerState*>(textureState.second.get())) {
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

        if (auto samplerState = static_cast<SamplerState*>(textureState.second.get())) {
          samplerState->bind(texture);
        }
        CLEAR_DIRTY(fragmentTextureStatesDirty_, index);
      }
    }
  }
}

void RenderCommandAdapter::unbindTexture(IContext& context,
                                         size_t textureUnit,
                                         TextureState& textureState) {
  if (auto* texture = static_cast<Texture*>(textureState.first)) {
    context.activeTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
    texture->unbind();
  }
}

void RenderCommandAdapter::unbindTextures(IContext& context,
                                          TextureStates& states,
                                          std::bitset<IGL_TEXTURE_SAMPLERS_MAX>& dirtyFlags) {
  for (size_t index = 0; index < states.size(); index++) {
    unbindTexture(context, index, states[index]);
    SET_DIRTY(dirtyFlags, index);
  }
}

GLenum RenderCommandAdapter::toMockWireframeMode(GLenum mode) const {
#if defined(IGL_OPENGL_ES)
  const auto pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());
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
  auto pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());
  if (pipelineState) {
    pipelineState->unbindVertexAttributes();
  }
}

void RenderCommandAdapter::unbindResources() {
  unbindTextures(getContext(), fragmentTextureStates_, fragmentTextureStatesDirty_);
  unbindTextures(getContext(), vertexTextureStates_, vertexTextureStatesDirty_);

  // Restore to default active texture
  getContext().activeTexture(GL_TEXTURE0);

  // TODO: unbind uniform blocks when we add support?

  auto depthStencilState = static_cast<DepthStencilState*>(depthStencilState_.get());
  if (depthStencilState) {
    depthStencilState->unbind();
    setDirty(StateMask::DEPTH_STENCIL);
  }

  auto pipelineState = static_cast<RenderPipelineState*>(pipelineState_.get());
  if (pipelineState) {
    unbindVertexAttributes();
    pipelineState->unbind();
    setDirty(StateMask::PIPELINE);
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
} // namespace opengl
} // namespace igl
