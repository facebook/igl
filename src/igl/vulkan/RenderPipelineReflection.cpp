/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "RenderPipelineReflection.h"

#include <cstring>

namespace igl::vulkan {

RenderPipelineReflection::RenderPipelineReflection() = default;

RenderPipelineReflection::RenderPipelineReflection(std::vector<BufferArgDesc> bufferArguments,
                                                   std::vector<SamplerArgDesc> samplerArguments,
                                                   std::vector<TextureArgDesc> textureArguments) :
  bufferArguments_(std::move(bufferArguments)),
  samplerArguments_(std::move(samplerArguments)),
  textureArguments_(std::move(textureArguments)) {}

RenderPipelineReflection::~RenderPipelineReflection() = default;

const std::vector<igl::BufferArgDesc>& RenderPipelineReflection::allUniformBuffers() const {
  return bufferArguments_;
}

const std::vector<igl::SamplerArgDesc>& RenderPipelineReflection::allSamplers() const {
  return samplerArguments_;
}

const std::vector<igl::TextureArgDesc>& RenderPipelineReflection::allTextures() const {
  return textureArguments_;
}

} // namespace igl::vulkan
