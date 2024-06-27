/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Uniform.h>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/IContext.h>
#include <unordered_map>
#include <vector>

namespace igl {
class ICommandBuffer;
namespace opengl {
class RenderPipelineState;

class UniformBuffer final : public Buffer {
 public:
  UniformBuffer(IContext& context,
                BufferDesc::BufferAPIHint requestedApiHints,
                BufferDesc::BufferType bufferType);
  ~UniformBuffer() override;

  Result upload(const void* data, const BufferRange& range) override;

  void* map(const BufferRange& range, Result* outResult) override;
  void unmap() override;

  [[nodiscard]] BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override {
    return 0;
  }

  [[nodiscard]] ResourceStorage storage() const noexcept override {
    return ResourceStorage::Shared;
  }

  [[nodiscard]] size_t getSizeInBytes() const override {
    return uniformData_.size();
  }

  void initialize(const BufferDesc& desc, Result* outResult) override;

  [[nodiscard]] Type getType() const noexcept override {
    return Type::Uniform;
  }

  // For openGL, additional information required to bind the uniform is provided when the buffer is
  // created within the igl::BufferDesc and igl::UniformBufferEntry (eg. offset, type, elementStride
  // and number of elements).
  //
  // However location information is provided when the uniform is bound via the
  // index parameter of igl::renderCommandEncoder bindBuffer, the offset parameter is then used to
  // lookup the information specified at buffer creation time. This allows the same uniform buffer
  // to be reused in multiple shaders at different locations as long as creation value information
  // (offset, type, etc.) does not change.
  static void bindUniform(IContext& context,
                          GLint shaderLocation,
                          UniformType uniformType,
                          const uint8_t* start,
                          size_t count);
  static void bindUniformArray(IContext& context,
                               GLint shaderLocation,
                               UniformType uniformType,
                               const uint8_t* start,
                               size_t numElements,
                               size_t stride);

 private:
  bool initializeCommon(const BufferDesc& desc, Result* outResult);
  void printUniforms(GLint program);

  // Copy of data from the client
  std::vector<uint8_t> uniformData_;

  bool isDynamic_; // TODO: Add support for dynamic uniforms
};

} // namespace opengl
} // namespace igl
