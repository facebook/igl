/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Buffer.h>
#include <igl/opengl/Memcpy.h>
#include <igl/opengl/UniformAdapter.h>
#include <igl/opengl/UniformBuffer.h>

namespace igl {
namespace opengl {

UniformAdapter::UniformAdapter(const IContext& context, PipelineType type) : pipelineType_(type) {
  const auto& deviceFeatures = context.deviceFeatures();
  maxUniforms_ = deviceFeatures.getMaxComputeUniforms();

  // NOTE: 32 "feels" right and yielded good results in MobileLab. Goal here is to minimize
  // number of resize's in the vector but not be unreasonably large.
  constexpr size_t kLikelyMaxiumNumUniforms = 32;
  uniforms_.reserve(kLikelyMaxiumNumUniforms);

  if (pipelineType_ == Render) {
    maxUniforms_ = deviceFeatures.getMaxVertexUniforms() + deviceFeatures.getMaxFragmentUniforms();
  } else {
    maxUniforms_ = deviceFeatures.getMaxComputeUniforms();
  }

  uniformBuffersDirtyMask_ = 0;
#if IGL_DEBUG
  uniformsDirty_.resize(maxUniforms_);
#endif
}

void UniformAdapter::shrinkUniformUsage() {
  static constexpr uint32_t maxUniformBytes = 32 * 1024;
  static constexpr uint32_t maxShrinkUniformCounter = 1000;
  if (uniformData_.size() > maxUniformBytes && usedUniformDataBytes_ < uniformData_.size() / 2) {
    shrinkUniformDataCounter_++;
    if (shrinkUniformDataCounter_ > maxShrinkUniformCounter) {
      uniformData_.resize(uniformData_.size() / 2);
      shrinkUniformDataCounter_ = 0;
    }
  } else {
    shrinkUniformDataCounter_ = 0;
  }
}

void UniformAdapter::clearUniformBuffers() {
  usedUniformDataBytes_ = 0;
  uniforms_.clear();
  uniformBuffersDirtyMask_ = 0;

#if IGL_DEBUG
  std::fill(uniformsDirty_.begin(), uniformsDirty_.end(), false);
#endif
}

void UniformAdapter::setUniform(const UniformDesc& uniformDesc,
                                const void* data,
                                Result* outResult) {
  auto location = uniformDesc.location;
  IGL_ASSERT_MSG(location >= 0, "Invalid uniformDesc->location passed to setUniform");

  // Early out if any of the parameters are invalid.
  if (location < 0 || location >= maxUniforms_ || !data) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    IGL_LOG_INFO_ONCE("IGL WARNING: Invalid parameters found for setUniform. Location (%d) \n",
                      location);
    return;
  }

  const std::ptrdiff_t typeSize = igl::sizeForUniformElementType(uniformDesc.type);
  std::ptrdiff_t length = uniformDesc.elementStride != 0
                              ? uniformDesc.elementStride
                              : igl::sizeForUniformType(uniformDesc.type);
  length *= uniformDesc.numElements;

  // Make sure typeSize is not 0 and is a power of 2
  if (!IGL_VERIFY((typeSize != 0) && ((typeSize - 1) & typeSize) == 0)) {
    Result::setResult(
        outResult, Result::Code::InvalidOperation, "typeSize is 0 or not a power of 2");
    IGL_LOG_INFO_ONCE("IGL WARNING: Invalid typeSize (%zu) is used. Found 0 or not power of 2\n",
                      typeSize);
    return;
  }

  // Calculate the next size-aligned offset. Since typeSize is always a power
  // of 2, ~(typeSize - 1) will mask off the unaligned bits. Since masking
  // bits off is like a subtraction, we need to add (typeSize - 1) to make
  // sure we are still moving forward in the address space.
  const std::ptrdiff_t dataOffset = (usedUniformDataBytes_ + (typeSize - 1)) & ~(typeSize - 1);

  // Make sure dataOffset is typeSize aligned
  if (!IGL_VERIFY((dataOffset & (typeSize - 1)) == 0)) {
    Result::setResult(
        outResult, Result::Code::InvalidOperation, "dataOffset is not typeSize aligned");
    IGL_LOG_INFO_ONCE(
        "IGL WARNING: Invalid dataOffset alignment(%td) for typeSize(%zu)\n", dataOffset, typeSize);
    return;
  }

  usedUniformDataBytes_ = dataOffset + length;

  if (usedUniformDataBytes_ > uniformData_.size()) {
    uniformData_.resize(usedUniformDataBytes_);
  }

  optimizedMemcpy(uniformData_.data() + dataOffset, (uint8_t*)data + uniformDesc.offset, length);

#if IGL_DEBUG
  // We don't catch duplicate uniforms set on a given location in production.
  // This is technically a client bug and we shouldn't be doing this sort of
  // error-checking, as we're in the inner loop of rendering.
  //
  // Instead, we assert in local dev builds to catch if we're setting uniform block
  // in same location previously set (in either uniform or block) during the draw call.
  IGL_ASSERT(!uniformsDirty_[location]);
  uniformsDirty_[location] = true;
#endif // IGL_DEBUG

  IGL_ASSERT(uniforms_.size() < maxUniforms_);
  uniforms_.emplace_back(uniformDesc, dataOffset);
  Result::setOk(outResult);
}

void UniformAdapter::setUniformBuffer(const std::shared_ptr<IBuffer>& buffer,
                                      size_t offset,
                                      int bindingIndex,
                                      Result* outResult) {
  IGL_ASSERT_MSG(bindingIndex >= 0, "invalid bindingIndex passed to setUniformBuffer");
  IGL_ASSERT_MSG(bindingIndex <= IGL_UNIFORM_BLOCKS_BINDING_MAX,
                 "Uniform buffer index is beyond max");
  IGL_ASSERT_MSG(buffer, "invalid buffer passed to setUniformBuffer");
  if (bindingIndex >= 0 && bindingIndex < IGL_UNIFORM_BLOCKS_BINDING_MAX && buffer) {
    uniformBufferBindingMap_[bindingIndex] = {buffer, offset};
    uniformBuffersDirtyMask_ |= 1 << bindingIndex;
    Result::setOk(outResult);
  } else {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
  }
}

void UniformAdapter::bindToPipeline(IContext& context) {
  // bind uniforms
  for (const auto& uniform : uniforms_) {
    const auto& uniformDesc = uniform.desc;
    IGL_ASSERT(uniformDesc.location >= 0);
    IGL_ASSERT_MSG(uniformData_.data(), "Uniform data must be non-null");
    auto start = uniformData_.data() + uniform.dataOffset;
    if (uniformDesc.numElements > 1 || uniformDesc.type == UniformType::Mat3x3) {
      IGL_ASSERT_MSG(uniformDesc.elementStride > 0,
                     "stride has to be larger than 0 for uniform at offset %zu",
                     uniformDesc.offset);
      UniformBuffer::bindUniformArray(context,
                                      uniformDesc.location,
                                      uniformDesc.type,
                                      start,
                                      uniformDesc.numElements,
                                      uniformDesc.elementStride);
    } else {
      UniformBuffer::bindUniform(context, uniformDesc.location, uniformDesc.type, start, 1);
    }
  }
  uniforms_.clear();
#if IGL_DEBUG
  std::fill(uniformsDirty_.begin(), uniformsDirty_.end(), false);
#endif

  // bind uniform block buffers
  for (size_t bindingIndex = 0; bindingIndex < IGL_UNIFORM_BLOCKS_BINDING_MAX; ++bindingIndex) {
    if (uniformBuffersDirtyMask_ & (1 << bindingIndex)) {
      auto uniformBinding = uniformBufferBindingMap_.at(bindingIndex);
      auto* bufferState = static_cast<UniformBlockBuffer*>(uniformBinding.first.get());
      IGL_ASSERT(bufferState);
      if (uniformBinding.second) {
        bufferState->bindRange(bindingIndex, uniformBinding.second, nullptr);
      } else {
        bufferState->bindBase(bindingIndex, nullptr);
      }
    }
  }
  uniformBuffersDirtyMask_ = 0;
}

} // namespace opengl
} // namespace igl
