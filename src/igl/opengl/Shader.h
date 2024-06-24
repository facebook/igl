/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdlib>
#include <igl/ComputePipelineState.h>
#include <igl/NameHandle.h>
#include <igl/RenderPipelineState.h>
#include <igl/Shader.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <unordered_map>

namespace igl {
class ICommandBuffer;
namespace opengl {

class ShaderModule : public WithContext, public IShaderModule {
  friend class Device;

 public:
  ~ShaderModule() override;
  Result create(const ShaderModuleDesc& desc);

  [[nodiscard]] inline GLenum getShaderType() const {
    return shaderType_;
  }

  [[nodiscard]] inline GLuint getShaderID() const {
    return shaderID_;
  }

  [[nodiscard]] inline size_t getHash() const {
    return hash_;
  }

  ShaderModule(IContext& context, ShaderModuleInfo info);

 private:
  // Type of shader (vertex, fragment, compute)
  GLenum shaderType_ = 0;

  // The GL shader object ID
  GLuint shaderID_ = 0;

  // Hash of the shader source
  size_t hash_ = 0;
};

class ShaderStages final : public IShaderStages, public WithContext {
  friend class PipelineState;

 public:
  explicit ShaderStages(const ShaderStagesDesc& desc, IContext& context);
  ~ShaderStages() override;

  Result create(const ShaderStagesDesc& /*desc*/);

  Result validate();
  void bind();
  void unbind();

  [[nodiscard]] GLuint getProgramID() const {
    return programID_;
  }

 private:
  void createRenderProgram(Result* result);
  void createComputeProgram(Result* result);
  std::string getProgramInfoLog(GLuint programID);

  // the GL shader program ID
  GLuint programID_ = 0;
};

} // namespace opengl
} // namespace igl
