/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Buffer.h>

#include <igl/CommandBuffer.h>
#include <igl/Device.h>
#include <igl/opengl/Errors.h>

namespace igl {
namespace opengl {

// ********************************
// ****  ArrayBuffer
// ********************************
// the base buffer object
ArrayBuffer::ArrayBuffer(IContext& context, BufferDesc::BufferAPIHint requestedApiHints) :
  Buffer(context, requestedApiHints) {
  iD_ = 0;
  size_ = 0;
  isDynamic_ = false;
}

ArrayBuffer::~ArrayBuffer() {
  if (iD_ != 0) {
    getContext().deleteBuffers(1, &iD_);
    getContext().unbindBuffer(target_);
    iD_ = 0;
  }
  size_ = 0;
  isDynamic_ = false;
}

// initialize a buffer with the given size
// if data is not null, copy the data into the buffer
// if the buffer is to be updated frequently, isDynamic should be set to true
void ArrayBuffer::initialize(const BufferDesc& desc, Result* outResult) {
  // static buffers must provide their data during creation, as they can't upload data later on
  GLenum usage = GL_DYNAMIC_DRAW;
  switch (desc.storage) {
  case ResourceStorage::Shared:
    usage = GL_DYNAMIC_DRAW;
    isDynamic_ = true;
    break;
  case ResourceStorage::Managed:
    usage = GL_STATIC_DRAW;
    isDynamic_ = false;
    break;
  case ResourceStorage::Private:
    usage = GL_STATIC_DRAW;
    isDynamic_ = false;
    break;
  default:
    break;
  }

  if (!isDynamic_ && desc.data == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "data is null");
    return;
  }

  getContext().genBuffers(1, &iD_);

  if (desc.type & BufferDesc::BufferTypeBits::Storage) {
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::Compute)) {
      target_ = GL_SHADER_STORAGE_BUFFER;
    } else {
      IGL_ASSERT_NOT_IMPLEMENTED();
    }
  } else if (desc.type & BufferDesc::BufferTypeBits::Uniform) {
    target_ = GL_UNIFORM_BUFFER;
  } else if (desc.type & BufferDesc::BufferTypeBits::Vertex) {
    target_ = GL_ARRAY_BUFFER;
  } else if (desc.type & BufferDesc::BufferTypeBits::Index) {
    target_ = GL_ELEMENT_ARRAY_BUFFER;
  } else {
    IGL_ASSERT_NOT_IMPLEMENTED();
  }

  size_ = desc.length;

  getContext().bindBuffer(target_, iD_);
  getContext().bufferData(target_, size_, desc.data, usage);

  // make sure the buffer was fully allocated
  GLint bufferSize = 0;
  getContext().getBufferParameteriv(target_, GL_BUFFER_SIZE, &bufferSize);

  getContext().bindBuffer(target_, 0);

  if (bufferSize != size_) {
    getContext().deleteBuffers(1, &iD_);
    iD_ = 0;
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "bufferSize != dataSize");
    return;
  }

  Result::setOk(outResult);
}

// upload data to the buffer at the given offset with the given size
Result ArrayBuffer::upload(const void* data, const BufferRange& range) {
  // static buffers can only upload data once during creation
  if (!isDynamic_) {
    return Result(Result::Code::InvalidOperation, "Can't upload to static buffers");
  }

  getContext().bindBuffer(target_, iD_);

  getContext().bufferSubData(target_, range.offset, range.size, data);

  getContext().bindBuffer(target_, 0);

  return Result();
}

void* ArrayBuffer::map(const BufferRange& range, Result* outResult) {
  if ((range.size + range.offset) > getSizeInBytes()) {
    Result::setResult(
        outResult, Result::Code::ArgumentOutOfRange, "map() size + offset must be <= buffer size");
    return nullptr;
  }

  bind();

  void* srcData = nullptr;
  srcData = getContext().mapBufferRange(target_, range.offset, range.size, GL_MAP_READ_BIT);
  IGL_ASSERT(srcData != nullptr);
  if (srcData == nullptr) {
    Result::setResult(outResult, Result::Code::InvalidOperation);
    return nullptr;
  }

  Result::setOk(outResult);
  return srcData;
}

void ArrayBuffer::unmap() {
  bind();
  getContext().unmapBuffer(target_);
}

// bind the buffer for access by the GPU
void ArrayBuffer::bind() {
  getContext().bindBuffer(target_, iD_);
}

void ArrayBuffer::unbind() {
  getContext().bindBuffer(target_, 0);
}

void ArrayBuffer::bindBase(IGL_MAYBE_UNUSED size_t index, Result* outResult) {
  if (target_ != GL_SHADER_STORAGE_BUFFER) {
    static const char* kErrorMsg = "Buffer should be GL_SHADER_STORAGE_BUFFER";
    IGL_REPORT_ERROR_MSG(1, kErrorMsg);
    Result::setResult(outResult, Result::Code::InvalidOperation, kErrorMsg);
    return;
  }
  getContext().bindBuffer(target_, iD_);
  getContext().bindBufferBase(target_, (GLuint)index, iD_);
  Result::setOk(outResult);
}

void ArrayBuffer::bindForTarget(GLenum target) {
  getContext().bindBuffer(target, iD_);
}

void UniformBlockBuffer::setBlockBinding(GLuint pid, GLuint blockIndex, GLuint bindingPoint) {
  getContext().uniformBlockBinding(pid, blockIndex, bindingPoint);
}

void UniformBlockBuffer::bindBase(size_t index, Result* outResult) {
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::UniformBlocks)) {
    if (target_ != GL_UNIFORM_BUFFER) {
      static const char* kErrorMsg = "Buffer should be GL_UNIFORM_BUFFER";
      IGL_REPORT_ERROR_MSG(1, kErrorMsg);
      Result::setResult(outResult, Result::Code::InvalidOperation, kErrorMsg);
      return;
    }
    getContext().bindBuffer(target_, iD_);
    getContext().bindBufferBase(target_, (GLuint)index, iD_);
    Result::setOk(outResult);
  } else {
    static const char* kErrorMsg = "Uniform Blocks are not supported";
    IGL_REPORT_ERROR_MSG(1, kErrorMsg);
    Result::setResult(outResult, Result::Code::Unimplemented, kErrorMsg);
  }
}

void UniformBlockBuffer::bindRange(size_t index, size_t offset, Result* outResult) {
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::UniformBlocks)) {
    if (target_ != GL_UNIFORM_BUFFER) {
      static const char* kErrorMsg = "Buffer should be GL_UNIFORM_BUFFER";
      IGL_REPORT_ERROR_MSG(1, kErrorMsg);
      Result::setResult(outResult, Result::Code::InvalidOperation, kErrorMsg);
      return;
    }
    getContext().bindBuffer(target_, iD_);
    IGL_ASSERT_MSG(
        offset < getSizeInBytes(), "Offset is invalid! (%d %d)", offset, getSizeInBytes());
    getContext().bindBufferRange(
        target_, (GLuint)index, iD_, (GLintptr)offset, getSizeInBytes() - offset);
    Result::setOk(outResult);
  } else {
    static const char* kErrorMsg = "Uniform Blocks are not supported";
    IGL_REPORT_ERROR_MSG(1, kErrorMsg);
    Result::setResult(outResult, Result::Code::Unimplemented, kErrorMsg);
  }
}

} // namespace opengl
} // namespace igl
