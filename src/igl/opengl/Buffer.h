/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Shader.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/WithContext.h>

namespace igl {
class ICommandBuffer;
namespace opengl {
class RenderPipelineState;

class Buffer : public WithContext, public IBuffer {
 public:
  enum class Type : uint8_t { Attribute, Uniform, UniformBlock };

  Buffer(IContext& context,
         BufferDesc::BufferAPIHint requestedApiHints,
         BufferDesc::BufferType bufferType) :
    WithContext(context), requestedApiHints_(requestedApiHints), bufferType_(bufferType) {}
  [[nodiscard]] uint64_t gpuAddress(size_t /*offset*/) const override {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return 0;
  }
  virtual void initialize(const BufferDesc& desc, Result* outResult) = 0;
  [[nodiscard]] virtual Type getType() const noexcept = 0;

  [[nodiscard]] BufferDesc::BufferAPIHint requestedApiHints() const noexcept override {
    return requestedApiHints_;
  }

  [[nodiscard]] BufferDesc::BufferType getBufferType() const override {
    return bufferType_;
  }

 private:
  BufferDesc::BufferAPIHint requestedApiHints_;
  BufferDesc::BufferType bufferType_ = 0;
};

class ArrayBuffer : public Buffer {
 public:
  ArrayBuffer(IContext& context,
              BufferDesc::BufferAPIHint requestedApiHints,
              BufferDesc::BufferType bufferType);
  ~ArrayBuffer() override;

  Result upload(const void* data, const BufferRange& range) override;

  void* map(const BufferRange& range, Result* outResult) override;
  void unmap() override;

  [[nodiscard]] BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override {
    return 0;
  }

  [[nodiscard]] ResourceStorage storage() const noexcept override {
    return ResourceStorage::Managed;
  }

  [[nodiscard]] size_t getSizeInBytes() const override {
    return size_;
  }

  IGL_INLINE GLuint getId() const noexcept {
    return iD_;
  }

  IGL_INLINE GLenum getTarget() const noexcept {
    return target_;
  }

  void initialize(const BufferDesc& desc, Result* outResult) override;

  void bind();
  void unbind();

  void bindBase(size_t index, Result* outResult);

  void bindForTarget(GLenum target);

  [[nodiscard]] Type getType() const noexcept override {
    return Type::Attribute;
  }

 protected:
  // the GL ID for this texture
  GLuint iD_;

  // the buffer target used by the GL glBufferXXX APIs
  // this must be set by each derived object during construction
  GLenum target_{};

 private:
  size_t size_;

  bool isDynamic_;
};

class UniformBlockBuffer : public ArrayBuffer {
 public:
  UniformBlockBuffer(IContext& context,
                     BufferDesc::BufferAPIHint requestedApiHints,
                     BufferDesc::BufferType bufferType) :
    ArrayBuffer(context, requestedApiHints, bufferType) {}

  [[nodiscard]] Type getType() const noexcept override {
    return Type::UniformBlock;
  }

  void setBlockBinding(GLuint pid, GLuint blockIndex, GLuint bindingPoint);
  void bindBase(size_t index, Result* outResult);
  void bindRange(size_t index, size_t offset, Result* outResult);

  [[nodiscard]] BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override {
    return BufferDesc::BufferAPIHintBits::UniformBlock;
  }
};

} // namespace opengl
} // namespace igl
