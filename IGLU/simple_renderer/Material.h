/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#pragma once

#include "ShaderProgram.h"
#include "ShaderUniforms.h"

#include <igl/IGL.h>
#include <string>

namespace iglu::material {

enum class DepthTestConfig {
  Disable,
  Enable,
  EnableNoWrite,
};

/// Aggregates all blend mode related configurations.
class BlendMode {
 public:
  igl::BlendFactor srcRGB;
  igl::BlendFactor dstRGB;
  igl::BlendOp opRGB;
  igl::BlendFactor srcAlpha;
  igl::BlendFactor dstAlpha;
  igl::BlendOp opAlpha;

  BlendMode();
  BlendMode(igl::BlendFactor src, igl::BlendFactor dst) :
    BlendMode(src, dst, igl::BlendOp::Add, src, dst, igl::BlendOp::Add) {}
  BlendMode(igl::BlendFactor srcRGB,
            igl::BlendFactor dstRGB,
            igl::BlendOp opRGB,
            igl::BlendFactor srcAlpha,
            igl::BlendFactor dstAlpha,
            igl::BlendOp opAlpha) :
    srcRGB(srcRGB),
    dstRGB(dstRGB),
    opRGB(opRGB),
    srcAlpha(srcAlpha),
    dstAlpha(dstAlpha),
    opAlpha(opAlpha) {}

  static BlendMode Opaque() {
    return {igl::BlendFactor::One, igl::BlendFactor::Zero};
  }
  static BlendMode Translucent() {
    return {igl::BlendFactor::SrcAlpha,
            igl::BlendFactor::OneMinusSrcAlpha,
            igl::BlendOp::Add,
            igl::BlendFactor::One,
            igl::BlendFactor::OneMinusSrcAlpha,
            igl::BlendOp::Add};
  }
  static BlendMode Additive() {
    return {igl::BlendFactor::SrcAlpha, igl::BlendFactor::One};
  }
  static BlendMode Premultiplied() {
    return {igl::BlendFactor::One, igl::BlendFactor::OneMinusSrcAlpha};
  }

  bool operator==(const BlendMode& other) const {
    return srcRGB == other.srcRGB && dstRGB == other.dstRGB && opRGB == other.opRGB &&
           srcAlpha == other.srcAlpha && dstAlpha == other.dstAlpha && opAlpha == other.opAlpha;
  }
};

/// Aggregates all configurations that affect how vertex data will be rendered. It also
/// simplifies render pipeline state manipulation.
///
/// A "material" is typically associated with artistic inputs to renderable objects. The
/// shader program and its inputs are the most obvious example of controlling the looks of a
/// renderable object, but there are other pipeline states that are relevant.
class Material final {
 public:
  std::string name = "<unnamed>";
  BlendMode blendMode = BlendMode::Opaque();
  igl::CullMode cullMode = igl::CullMode::Back;

  [[nodiscard]] std::shared_ptr<ShaderProgram> shaderProgram() const;
  void setShaderProgram(igl::IDevice& device, const std::shared_ptr<ShaderProgram>& shaderProgram);

  /// There's a 1-to-1 correspondence between the ShaderProgram and the ShaderUniforms object.
  /// Don't cache this returned object, as changing the shader program will create a new one.
  [[nodiscard]] ShaderUniforms& shaderUniforms() const;

  [[nodiscard]] DepthTestConfig depthTestConfig() const;
  void setDepthTestConfig(igl::IDevice& device, const DepthTestConfig& config);

  /// Populates a pipeline descriptor for drawing using this Material.
  void populatePipelineDescriptor(igl::RenderPipelineDesc& pipelineDesc) const;

  /// Binds all relevant states in 'encoder' in preparation for drawing.
  void bind(igl::IDevice& device,
            igl::IRenderPipelineState& pipelineState,
            igl::IRenderCommandEncoder& commandEncoder);

  explicit Material(igl::IDevice& device, std::string name = "<unnamed>");
  ~Material() = default;

 private:
  std::shared_ptr<ShaderProgram> _shaderProgram;
  std::unique_ptr<ShaderUniforms> _shaderUniforms;
  std::shared_ptr<igl::IDepthStencilState> _depthState;
  DepthTestConfig _depthTestConfig = DepthTestConfig::Disable;
};

} // namespace iglu::material
