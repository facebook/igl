/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/ComputePipelineState.h>
#include <igl/NameHandle.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderPipelineReflection.h>
#include <igl/opengl/Shader.h>
#include <unordered_map>

namespace igl::opengl {
class VertexInputState;
class Texture;
class Buffer;
class Device;

class ComputePipelineState final : public WithContext, public IComputePipelineState {
  friend class Device;

 public:
  explicit ComputePipelineState(IContext& context);
  ~ComputePipelineState() override;

  Result create(const ComputePipelineDesc& desc);
  void bind();
  void unbind();
  Result bindTextureUnit(size_t unit, Texture* texture);
  Result bindBuffer(size_t unit, Buffer* buffer);

  std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override;

  [[nodiscard]] int getIndexByName(const NameHandle& name) const override;

  bool getIsUsingShaderStorageBuffers() {
    return usingShaderStorageBuffers_;
  }

 private:
  using ComputePipelineReflection = RenderPipelineReflection;

  std::array<GLint, IGL_VERTEX_BUFFER_MAX> bufferUnitMap_{};
  std::array<GLint, IGL_TEXTURE_SAMPLERS_MAX> imageUnitMap_{};

  std::shared_ptr<ShaderStages> shaderStages_;
  std::shared_ptr<ComputePipelineReflection> reflection_;

  bool usingShaderStorageBuffers_ = false;
};

} // namespace igl::opengl
