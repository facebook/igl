/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ComputeCommandEncoder.h>

#include <algorithm>
#include <array>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/ComputeCommandAdapter.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/SamplerState.h>
#include <igl/opengl/Shader.h>
#include <igl/opengl/Texture.h>

namespace igl {
namespace opengl {

///----------------------------------------------------------------------------
/// MARK: - ComputeCommandEncoder

ComputeCommandEncoder::ComputeCommandEncoder(IContext& context) : WithContext(context) {
  auto& oglContext = getContext();

  auto& pool = oglContext.getComputeAdapterPool();
  if (pool.empty()) {
    adapter_ = std::make_unique<ComputeCommandAdapter>(oglContext);
  } else {
    adapter_ = std::move(pool[pool.size() - 1]);
    pool.pop_back();
  }
}

ComputeCommandEncoder::~ComputeCommandEncoder() = default;

void ComputeCommandEncoder::endEncoding() {
  if (IGL_VERIFY(adapter_)) {
    adapter_->endEncoding();
    getContext().getComputeAdapterPool().push_back(std::move(adapter_));
  }
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setPipelineState(pipelineState);
  }
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& threadgroupSize) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->dispatchThreadGroups(threadgroupCount, threadgroupSize);
  }
}

void ComputeCommandEncoder::pushDebugGroupLabel(const std::string& label,
                                                const igl::Color& /*color*/) const {
  IGL_ASSERT(!label.empty());
  getContext().pushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, label.length(), label.c_str());
}

void ComputeCommandEncoder::insertDebugEventLabel(const std::string& label,
                                                  const igl::Color& /*color*/) const {
  IGL_ASSERT(!label.empty());
  getContext().debugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                                  GL_DEBUG_TYPE_MARKER,
                                  0,
                                  GL_DEBUG_SEVERITY_LOW,
                                  label.length(),
                                  label.c_str());
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  getContext().popDebugGroup();
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& uniformDesc, const void* data) {
  IGL_ASSERT_MSG(uniformDesc.location >= 0,
                 "Invalid location passed to bindUniformBuffer: %d",
                 uniformDesc.location);
  IGL_ASSERT_MSG(data != nullptr, "Data cannot be null");
  if (IGL_VERIFY(adapter_) && data) {
    adapter_->setUniform(uniformDesc, data);
  }
}

void ComputeCommandEncoder::bindTexture(size_t index, ITexture* texture) {
  if (IGL_VERIFY(adapter_)) {
    adapter_->setTexture(texture, index);
  }
}

void ComputeCommandEncoder::bindBuffer(size_t index,
                                       const std::shared_ptr<IBuffer>& buffer,
                                       size_t offset) {
  if (IGL_VERIFY(adapter_) && buffer) {
    auto glBuffer = std::static_pointer_cast<Buffer>(buffer);
    adapter_->setBuffer(glBuffer, offset, static_cast<int>(index));
  }
}

void ComputeCommandEncoder::bindBytes(size_t /*index*/, const void* /*data*/, size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindPushConstants(size_t /*offset*/,
                                              const void* /*data*/,
                                              size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

} // namespace opengl
} // namespace igl
