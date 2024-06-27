/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/NameHandle.h>
#include <igl/RenderPipelineState.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderPipelineReflection.h>
#include <igl/opengl/Shader.h>
#include <unordered_map>

namespace igl::opengl {

struct BlendMode {
  GLenum blendOpColor;
  GLenum blendOpAlpha;
  GLenum srcColor;
  GLenum dstColor;
  GLenum srcAlpha;
  GLenum dstAlpha;
};

class Device;

class RenderPipelineState final : public WithContext, public IRenderPipelineState {
  friend class Device;

  Result create();

 public:
  explicit RenderPipelineState(IContext& context,
                               const RenderPipelineDesc& desc,
                               Result* outResult);
  ~RenderPipelineState() override;

  void bind();
  void unbind();
  Result bindTextureUnit(size_t unit, uint8_t bindTarget);

  void bindVertexAttributes(size_t bufferIndex, size_t offset);
  void unbindVertexAttributes();

  [[nodiscard]] bool matchesShaderProgram(const RenderPipelineState& rhs) const;
  [[nodiscard]] bool matchesVertexInputState(const RenderPipelineState& rhs) const;

  [[nodiscard]] int getIndexByName(const NameHandle& name, ShaderStage stage) const override;
  [[nodiscard]] int getIndexByName(const std::string& name, ShaderStage stage) const override;

  [[nodiscard]] int getUniformBlockBindingPoint(const NameHandle& uniformBlockName) const;
  std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() override;
  void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) override;

  static GLenum convertBlendOp(BlendOp value);
  static GLenum convertBlendFactor(BlendFactor value);
  [[nodiscard]] CullMode getCullMode() const {
    return desc_.cullMode;
  }
  [[nodiscard]] WindingMode getWindingMode() const {
    return desc_.frontFaceWinding;
  }
  [[nodiscard]] PolygonFillMode getPolygonFillMode() const {
    return desc_.polygonFillMode;
  }

  std::unordered_map<int, size_t>& uniformBlockBindingMap();

 private:
  // Tracks a list of attribute locations associated with a bufferIndex
  std::vector<int> bufferAttribLocations_[IGL_VERTEX_BUFFER_MAX];

  std::shared_ptr<RenderPipelineReflection> reflection_;
  std::unordered_map<size_t, size_t> vertexTextureUnitRemap;
  std::array<GLint, IGL_TEXTURE_SAMPLERS_MAX> unitSamplerLocationMap_{};
  std::unordered_map<int, size_t> uniformBlockBindingMap_;
  std::array<GLboolean, 4> colorMask_ = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  std::vector<int> activeAttributesLocations_;
  BlendMode blendMode_ = {GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO};
  bool blendEnabled_ = false;
};

} // namespace igl::opengl
