/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <utility>
#include <vector>
#include <igl/RenderPipelineReflection.h>
#include <igl/webgpu/WgslReflection.h>

namespace igl::webgpu {

/// @brief IRenderPipelineReflection built from WGSL reflection with bind convention v1: a buffer
/// at IGL index i is @group(1) @binding(i); texture unit i is @group(0) @binding(2i) and its
/// sampler @binding(2i+1); storage texture i is @group(2) @binding(i).
class RenderPipelineReflection final : public IRenderPipelineReflection {
 public:
  explicit RenderPipelineReflection(
      const std::vector<std::pair<ShaderStage, const WgslReflection*>>& stages);

  /// The IGL index of the buffer, texture or sampler named `name` in `stage`, or -1.
  [[nodiscard]] int getIndexByName(const std::string& name, ShaderStage stage) const;

  [[nodiscard]] const std::vector<BufferArgDesc>& allUniformBuffers() const override {
    return buffers_;
  }
  [[nodiscard]] const std::vector<SamplerArgDesc>& allSamplers() const override {
    return samplers_;
  }
  [[nodiscard]] const std::vector<TextureArgDesc>& allTextures() const override {
    return textures_;
  }

 private:
  std::vector<BufferArgDesc> buffers_;
  std::vector<SamplerArgDesc> samplers_;
  std::vector<TextureArgDesc> textures_;
};

} // namespace igl::webgpu
