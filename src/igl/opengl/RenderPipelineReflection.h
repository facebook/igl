/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderPipelineReflection.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/Shader.h>
#include <unordered_map>

namespace igl::opengl {

class RenderPipelineReflection final : public IRenderPipelineReflection {
 public:
  struct UniformDesc {
    GLsizei size;
    GLint location;
    GLenum type;

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    UniformDesc(GLsizei s, GLint loc, GLenum t) : size(s), location(loc), type(t) {}
  };

  struct UniformBlockDesc {
    struct UniformBlockMemberDesc {
      GLsizei size = 0;
      GLenum type = GL_NONE;
      GLint offset = 0;
    };

    GLint size{};
    GLint blockIndex{};
    GLint bindingIndex = -1; // the block binding location, when set directly in the shader
    std::unordered_map<NameHandle, UniformBlockMemberDesc> members;
  };

  [[nodiscard]] const std::vector<BufferArgDesc>& allUniformBuffers() const override;
  [[nodiscard]] const std::vector<SamplerArgDesc>& allSamplers() const override;
  [[nodiscard]] const std::vector<TextureArgDesc>& allTextures() const override;

  [[nodiscard]] int getIndexByName(const NameHandle& name) const;

  [[nodiscard]] const std::unordered_map<NameHandle, UniformDesc>& getUniformDictionary() const {
    return uniformDictionary_;
  }

  [[nodiscard]] const std::unordered_map<NameHandle, UniformBlockDesc>& getUniformBlocksDictionary()
      const {
    return uniformBlocksDictionary_;
  }

  std::unordered_map<NameHandle, UniformBlockDesc>& getUniformBlocksDictionary() {
    return uniformBlocksDictionary_;
  }

  [[nodiscard]] const std::unordered_map<std::string, int>& getAttributeDictionary() const {
    return attributeDictionary_;
  }

  [[nodiscard]] const std::unordered_map<NameHandle, int>& getShaderStorageBufferObjectDictionary()
      const {
    return shaderStorageBufferObjectDictionary_;
  }

  RenderPipelineReflection(IContext& context, const ShaderStages& stages);
  ~RenderPipelineReflection() override;

 private:
  std::unordered_map<NameHandle, UniformDesc> uniformDictionary_;
  std::unordered_map<NameHandle, UniformBlockDesc> uniformBlocksDictionary_;
  std::unordered_map<std::string, int> attributeDictionary_;
  std::unordered_map<NameHandle, int> shaderStorageBufferObjectDictionary_;

  void generateUniformDictionary(IContext& context, GLuint pid);
  void generateUniformBlocksDictionary(IContext& context, GLuint pid);
  void generateShaderStorageBufferObjectDictionary(IContext& context, GLuint pid);
  void generateAttributeDictionary(IContext& context, GLuint pid);

  void cacheDescriptors();
  std::vector<BufferArgDesc> bufferArguments_;
  std::vector<SamplerArgDesc> samplerArguments_;
  std::vector<TextureArgDesc> textureArguments_;
};

} // namespace igl::opengl
