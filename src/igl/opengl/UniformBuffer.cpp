/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/UniformBuffer.h>

#include <cstring> // for memcpy()
#include <igl/Common.h>
#include <igl/IGLSafeC.h>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/RenderPipelineState.h>
#include <igl/opengl/Shader.h>
#include <memory>

namespace igl::opengl {
namespace {
// There's a non-zero chance of unexpected behavior if limit is larger than available stack size
const size_t kAllocSizeLimit = 512;
} // namespace
enum class UniformBaseType { Invalid = 0, Boolean, Int, Float, FloatMatrix };

template<typename T>
using ArrayHolder = std::unique_ptr<T[], void (*)(void*)>;
// This can't be a function because alloc result goes away on function return
#define IGL_MAYBE_STACK_ALLOC(Type, count)                                         \
  (sizeof(Type) * count) > kAllocSizeLimit                                         \
      ? ArrayHolder<Type>(reinterpret_cast<Type*>(malloc(sizeof(Type) * count)),   \
                          [](auto* addr) { free(reinterpret_cast<Type*>(addr)); }) \
      : ArrayHolder<Type>(reinterpret_cast<Type*>(alloca(sizeof(Type) * count)), [](auto*) {})

// ********************************
// ****  Buffer
// ********************************
// the base buffer object
UniformBuffer::UniformBuffer(IContext& context,
                             BufferDesc::BufferAPIHint requestedApiHints,
                             BufferDesc::BufferType bufferType) :
  Buffer(context, requestedApiHints, bufferType) {
  isDynamic_ = false;
}

UniformBuffer::~UniformBuffer() {
  isDynamic_ = false;
}

bool UniformBuffer::initializeCommon(const BufferDesc& desc, Result* outResult) {
  bool success = true;
  isDynamic_ = false;

  if (desc.data == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "Data in uniform desc is null");
    success = false;
  } else if (desc.length <= 0) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "Size of data in uniform desc (length) needs to be larger than 0");
    success = false;
  }
  return success;
}

// initialize a buffer with the given size
// if data is not null, copy the data into the buffer
// if the buffer is to be updated frequently, isDynamic should be set to true
void UniformBuffer::initialize(const BufferDesc& desc, Result* outResult) {
  if (!initializeCommon(desc, outResult)) {
    return;
  }

  uniformData_.resize(desc.length);
  memcpy(uniformData_.data(), desc.data, desc.length);

  Result::setOk(outResult);
}

// upload data to the buffer at the given offset with the given size
Result UniformBuffer::upload(const void* data, const BufferRange& range) {
  if (!IGL_VERIFY(range.offset + range.size <= getSizeInBytes())) {
    return Result{Result::Code::ArgumentOutOfRange, "Range size is larger than data size"};
  }

  checked_memcpy_offset(uniformData_.data(), uniformData_.size(), range.offset, data, range.size);

  return Result();
}

void* UniformBuffer::map(const BufferRange& range, Result* outResult) {
  if (getSizeInBytes() < (range.size + range.offset)) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "map() size + offset must be less than buffer size");
    return nullptr;
  }

  Result::setOk(outResult);
  return uniformData_.data() + range.offset;
}

void UniformBuffer::unmap() {}

void UniformBuffer::printUniforms(GLint program) {
  GLint i;
  GLint count;

  GLint size; // size of the variable
  GLenum type; // type of the variable (float, vec3 or mat4, etc)

  const GLsizei bufSize = 16; // maximum name length
  GLchar name[bufSize]; // variable name in GLSL
  GLsizei length; // name length

  getContext().getProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

  IGL_LOG_DEBUG("Active Uniforms: %d\n", count);

  for (i = 0; i < count; i++) {
    getContext().getActiveUniform(program, (GLuint)i, bufSize, &length, &size, &type, name);

    IGL_LOG_DEBUG("Uniform #%d Type: %u Name: %s\n", i, type, name);
  }
}

void UniformBuffer::bindUniform(IContext& context,
                                GLint shaderLocation,
                                UniformType uniformType,
                                const uint8_t* start,
                                size_t stCount) {
  if (IGL_VERIFY(shaderLocation >= 0)) {
    // If a glerror is hit within and of the getContext().uniform*** methods,
    // renderCommandEncoder->bindBuffer()'s index parameter likely does not map to the correct
    // location in the shader
    auto* uniformFloats = (GLfloat*)(start);
    auto* uniformInts = (GLint*)(start);
    auto count = static_cast<GLsizei>(stCount);
    switch (uniformType) {
    case UniformType::Int:
      context.uniform1iv(shaderLocation, count, uniformInts);
      break;
    case UniformType::Int2:
      context.uniform2iv(shaderLocation, count, uniformInts);
      break;
    case UniformType::Int3:
      context.uniform3iv(shaderLocation, count, uniformInts);
      break;
    case UniformType::Int4:
      context.uniform4iv(shaderLocation, count, uniformInts);
      break;
    case UniformType::Boolean: {
      // UniformType::Boolean is 1 byte, and at least for this case, IGL
      // expects the data to be packed. However, since glUniform1*() expects
      // each boolean to be passed in as GLint, we unpack the byte array
      // into GLint array.
      const std::unique_ptr<GLint[]> boolArray(new GLint[count]);
      for (size_t i = 0; i < count; i++) {
        boolArray[i] = static_cast<int>(!!*(start + i));
      }
      context.uniform1iv(shaderLocation, count, boolArray.get());

      break;
    }
    case UniformType::Float:
      context.uniform1fv(shaderLocation, count, uniformFloats);
      break;
    case UniformType::Float2:
      context.uniform2fv(shaderLocation, count, uniformFloats);
      break;
    case UniformType::Float3:
      context.uniform3fv(shaderLocation, count, uniformFloats);
      break;
    case UniformType::Float4:
      context.uniform4fv(shaderLocation, count, uniformFloats);
      break;
    case UniformType::Mat2x2:
      context.uniformMatrix2fv(shaderLocation, count, 0u, uniformFloats);
      break;
    case UniformType::Mat3x3:
      context.uniformMatrix3fv(shaderLocation, count, 0u, uniformFloats);
      break;
    case UniformType::Mat4x4:
      context.uniformMatrix4fv(shaderLocation, count, 0u, uniformFloats);
      break;
    case UniformType::Invalid:
      IGL_ASSERT_MSG(false, "Invalid Uniform Type");
      return;
    }
  }
}

void UniformBuffer::bindUniformArray(IContext& context,
                                     GLint shaderLocation,
                                     UniformType uniformType,
                                     const uint8_t* start,
                                     size_t numElements,
                                     size_t stride) {
  const size_t packedSize = igl::sizeForUniformType(uniformType);
  size_t primitivesPerElement = 0;
  UniformBaseType baseType;
  if (packedSize == stride) {
    UniformBuffer::bindUniform(context, shaderLocation, uniformType, start, numElements);
  } else {
    switch (uniformType) {
    case UniformType::Boolean:
      baseType = UniformBaseType::Boolean;
      break;
    case UniformType::Int:
      primitivesPerElement = 1;
      baseType = UniformBaseType::Int;
      break;
    case UniformType::Int2:
      primitivesPerElement = 2;
      baseType = UniformBaseType::Int;
      break;
    case UniformType::Int3:
      primitivesPerElement = 3;
      baseType = UniformBaseType::Int;
      break;
    case UniformType::Int4:
      primitivesPerElement = 4;
      baseType = UniformBaseType::Int;
      break;
    case UniformType::Float:
      primitivesPerElement = 1;
      baseType = UniformBaseType::Float;
      break;
    case UniformType::Float2:
      primitivesPerElement = 2;
      baseType = UniformBaseType::Float;
      break;
    case UniformType::Float3:
      primitivesPerElement = 3;
      baseType = UniformBaseType::Float;
      break;
    case UniformType::Float4:
      primitivesPerElement = 4;
      baseType = UniformBaseType::Float;
      break;
    case UniformType::Mat2x2:
      primitivesPerElement = 2;
      baseType = UniformBaseType::FloatMatrix;
      break;
    case UniformType::Mat3x3:
      primitivesPerElement = 3;
      baseType = UniformBaseType::FloatMatrix;
      break;
    case UniformType::Mat4x4:
      primitivesPerElement = 4;
      baseType = UniformBaseType::FloatMatrix;
      break;
    case UniformType::Invalid:
      IGL_ASSERT_MSG(false, "Invalid Uniform Type");
      return;
    }
    switch (baseType) {
    case UniformBaseType::Boolean: {
      auto packedIntArray = IGL_MAYBE_STACK_ALLOC(GLint, numElements);
      for (int i = 0; i < numElements; i++) {
        packedIntArray[i] = static_cast<int>(!!*(start));
        start += stride;
      }
      UniformBuffer::bindUniform(
          context, shaderLocation, UniformType::Int, (uint8_t*)packedIntArray.get(), numElements);
      break;
    }
    case UniformBaseType::Int: {
      auto packedIntArray = IGL_MAYBE_STACK_ALLOC(GLint, primitivesPerElement * numElements);
      for (int i = 0; i < numElements; i++) {
        optimizedMemcpy(
            &packedIntArray[i * primitivesPerElement], start, primitivesPerElement * sizeof(GLint));
        start += stride;
      }
      UniformBuffer::bindUniform(
          context, shaderLocation, uniformType, (uint8_t*)packedIntArray.get(), numElements);
      break;
    }
    case UniformBaseType::Float: {
      auto packedFloatArray = IGL_MAYBE_STACK_ALLOC(GLfloat, primitivesPerElement * numElements);
      for (int i = 0; i < numElements; i++) {
        optimizedMemcpy(&packedFloatArray[i * primitivesPerElement],
                        start,
                        primitivesPerElement * sizeof(GLfloat));
        start += stride;
      }
      UniformBuffer::bindUniform(
          context, shaderLocation, uniformType, (uint8_t*)packedFloatArray.get(), numElements);
      break;
    }
    case UniformBaseType::FloatMatrix: {
      auto packedFloatArray =
          IGL_MAYBE_STACK_ALLOC(GLfloat, primitivesPerElement * primitivesPerElement * numElements);
      for (int i = 0; i < numElements; i++) {
        for (int j = 0; j < primitivesPerElement; j++) {
          const size_t bytesToCopy = primitivesPerElement * sizeof(GLfloat);
          memcpy(&packedFloatArray[i * primitivesPerElement * primitivesPerElement +
                                   j * primitivesPerElement],
                 start,
                 bytesToCopy);
          start += (stride / primitivesPerElement);
        }
      }
      UniformBuffer::bindUniform(
          context, shaderLocation, uniformType, (uint8_t*)packedFloatArray.get(), numElements);
    } break;
    default:
      return;
    }
  }
}
} // namespace igl::opengl
