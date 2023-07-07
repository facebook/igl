/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/RenderPipelineReflection.h>
#include <igl/Shader.h>
#include <map>
#include <vector>

namespace igl {
namespace metal {

class RenderPipelineReflection final : public IRenderPipelineReflection {
 public:
  explicit RenderPipelineReflection(MTLRenderPipelineReflection* /*refl*/);
  ~RenderPipelineReflection() override;

  int getIndexByName(const std::string& name, ShaderStage sh) const;

  const std::vector<BufferArgDesc>& allUniformBuffers() const override;
  const std::vector<SamplerArgDesc>& allSamplers() const override;
  const std::vector<TextureArgDesc>& allTextures() const override;

 private:
  struct ArgIndex {
    ArgIndex(int argumentIndex, MTLArgumentType argumentType, size_t locationInArray) :
      argumentIndex(argumentIndex), argumentType(argumentType), locationInArray(locationInArray) {}
    int argumentIndex;
    MTLArgumentType argumentType;
    size_t locationInArray; /// position of this argument in the corresponding array
  };

  bool createArgDesc(MTLArgument* arg, ShaderStage sh);
  const std::map<std::string, ArgIndex>& getDictionary(ShaderStage sh) const;

  std::map<std::string, ArgIndex> vertexArgDictionary_;
  std::map<std::string, ArgIndex> fragmentArgDictionary_;

  std::vector<BufferArgDesc> bufferArguments_;
  std::vector<SamplerArgDesc> samplerArguments_;
  std::vector<TextureArgDesc> textureArguments_;
};

} // namespace metal
} // namespace igl
