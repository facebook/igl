/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ComputeCommandAdapter.h>

#include <algorithm>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/ComputePipelineState.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/Texture.h>
#include <igl/opengl/VertexInputState.h>

#define SET_DIRTY(dirtyMap, index) dirtyMap.set(index)
#define CLEAR_DIRTY(dirtyMap, index) dirtyMap.reset(index)
#define IS_DIRTY(dirtyMap, index) dirtyMap[index]

namespace igl::opengl {

ComputeCommandAdapter::ComputeCommandAdapter(IContext& context) :
  WithContext(context), uniformAdapter_(context, UniformAdapter::PipelineType::Compute) {}

void ComputeCommandAdapter::clearTextures() {}

void ComputeCommandAdapter::setTexture(ITexture* texture, uint32_t index) {
  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    return;
  }
  textureStates_[index] = texture;
  SET_DIRTY(textureStatesDirty_, index);
}

void ComputeCommandAdapter::clearBuffers() {
  buffersDirty_.reset();
}

void ComputeCommandAdapter::setBuffer(Buffer* buffer, size_t offset, uint32_t index) {
  IGL_DEBUG_ASSERT(index < IGL_BUFFER_BINDINGS_MAX,
                   "Buffer index is beyond max, may want to increase limit");
  if (index < uniformAdapter_.getMaxUniforms() && buffer) {
    buffers_[index] = {buffer, offset};
    SET_DIRTY(buffersDirty_, index);
  }
}

void ComputeCommandAdapter::clearUniformBuffers() {
  uniformAdapter_.clearUniformBuffers();
}

void ComputeCommandAdapter::setUniform(const UniformDesc& uniformDesc,
                                       const void* data,
                                       Result* outResult) {
  uniformAdapter_.setUniform(uniformDesc, data, outResult);
}

void ComputeCommandAdapter::setBlockUniform(Buffer* buffer,
                                            size_t offset,
                                            size_t size,
                                            int index,
                                            Result* outResult) {
  uniformAdapter_.setUniformBuffer(buffer, offset, size, index, outResult);
}

void ComputeCommandAdapter::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/) {
  willDispatch();
  getContext().dispatchCompute(static_cast<GLuint>(threadgroupCount.width),
                               static_cast<GLuint>(threadgroupCount.height),
                               static_cast<GLuint>(threadgroupCount.depth));
  didDispatch();
}

void ComputeCommandAdapter::setPipelineState(
    const std::shared_ptr<IComputePipelineState>& newValue) {
  if (pipelineState_) {
    clearDependentResources(newValue);
  }
  pipelineState_ = newValue;
  setDirty(StateMask::PIPELINE);
}

void ComputeCommandAdapter::clearDependentResources(
    const std::shared_ptr<IComputePipelineState>& newValue) {}

void ComputeCommandAdapter::willDispatch() {
  Result ret;
  auto* pipelineState = static_cast<ComputePipelineState*>(pipelineState_.get());

  IGL_DEBUG_ASSERT(pipelineState, "ComputePipelineState is nullptr");
  if (pipelineState == nullptr) {
    return;
  }

  for (uint32_t bufferIndex = 0; bufferIndex < IGL_BUFFER_BINDINGS_MAX; ++bufferIndex) {
    if (!IS_DIRTY(buffersDirty_, bufferIndex)) {
      continue;
    }
    auto& bufferState = buffers_[bufferIndex];
    ret = pipelineState->bindBuffer(bufferIndex, bufferState.resource);
    CLEAR_DIRTY(buffersDirty_, bufferIndex);
    if (!ret.isOk()) {
      IGL_LOG_INFO_ONCE(ret.message.c_str());
      continue;
    }
  }

  if (isDirty(StateMask::PIPELINE)) {
    pipelineState->bind();
    clearDirty(StateMask::PIPELINE);
  }

  // Bind uniforms to be used for compute
  uniformAdapter_.bindToPipeline(getContext());

  for (size_t index = 0; index < textureStates_.size(); index++) {
    if (!IS_DIRTY(textureStatesDirty_, index)) {
      continue;
    }
    auto& textureState = textureStates_[index];
    if (auto* texture = static_cast<Texture*>(textureState)) {
      ret = pipelineState->bindTextureUnit(index, texture);
      CLEAR_DIRTY(textureStatesDirty_, index);
      if (!ret.isOk()) {
        IGL_LOG_INFO_ONCE(ret.message.c_str());
        continue;
      }
    }
  }
}

void ComputeCommandAdapter::didDispatch() {
  getContext().memoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

  if (pipelineState_ == nullptr) {
    return;
  }
  auto* pipelineState = static_cast<ComputePipelineState*>(pipelineState_.get());
  IGL_DEBUG_ASSERT(pipelineState, "ComputePipelineState is nullptr");
  if (pipelineState == nullptr) {
    return;
  }
  if (pipelineState->getIsUsingShaderStorageBuffers()) {
    getContext().memoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT |
                               GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
  }
}

void ComputeCommandAdapter::endEncoding() {
  pipelineState_ = nullptr;
  textureStates_ = TextureStates();

  buffersDirty_.reset();
  textureStatesDirty_.reset();
  dirtyStateBits_ = EnumToValue(StateMask::NONE);

  uniformAdapter_.shrinkUniformUsage();
  uniformAdapter_.clearUniformBuffers();
}

} // namespace igl::opengl
