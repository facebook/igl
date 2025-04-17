/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#pragma once

#include <memory>
#include <string>
#include <igl/IGL.h>

namespace iglu::material {

/// Aggregates a vertex and a fragment module to extract shader reflection
/// information that can be used ahead of drawing.
class ShaderProgram final {
 public:
  /// Retrieve shader reflection information. This is particularly useful in
  /// scenarios where the application can't make fixed assumptions about the
  /// layout of the uniforms within a shader.
  [[nodiscard]] const igl::IRenderPipelineReflection& renderPipelineReflection() const;

  /// Populates a pipeline descriptor for drawing using this shader program.
  void populatePipelineDescriptor(igl::RenderPipelineDesc& pipelineDesc) const;

  ShaderProgram(igl::IDevice& device,
                std::shared_ptr<igl::IShaderModule> vertexShader,
                std::shared_ptr<igl::IShaderModule> fragmentShader,
                std::shared_ptr<igl::IVertexInputState> vis = nullptr,
                igl::Result* outResult = nullptr);
  ShaderProgram(igl::IDevice& device,
                std::shared_ptr<igl::IShaderStages> shaderStages,
                std::shared_ptr<igl::IVertexInputState> vis = nullptr,
                igl::Result* outResult = nullptr);
  ~ShaderProgram() = default;

 private:
  void init(igl::IDevice& device,
            std::shared_ptr<igl::IVertexInputState> vis,
            igl::Result* outResult);

  std::shared_ptr<igl::IShaderStages> _shaderStages;
  std::shared_ptr<igl::IRenderPipelineReflection> _reflection;
};

} // namespace iglu::material
