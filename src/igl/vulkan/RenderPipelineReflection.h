/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>
#include <igl/RenderPipelineReflection.h>
#include <igl/vulkan/ShaderModule.h>

namespace igl::vulkan {

/// @brief This is an empty class for now since Vulkan doesn't have a built-in reflection system. It
/// implements the igl::IRenderPipelineReflection interface
class RenderPipelineReflection final : public IRenderPipelineReflection {
 public:
  [[nodiscard]] const std::vector<BufferArgDesc>& allUniformBuffers() const override;
  [[nodiscard]] const std::vector<SamplerArgDesc>& allSamplers() const override;
  [[nodiscard]] const std::vector<TextureArgDesc>& allTextures() const override;

  RenderPipelineReflection();
  RenderPipelineReflection(std::vector<BufferArgDesc> bufferArguments,
                           std::vector<SamplerArgDesc> samplerArguments,
                           std::vector<TextureArgDesc> textureArguments);
  ~RenderPipelineReflection() override;

 private:
  std::vector<BufferArgDesc> bufferArguments_;
  std::vector<SamplerArgDesc> samplerArguments_;
  std::vector<TextureArgDesc> textureArguments_;
};

} // namespace igl::vulkan
