/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderPipelineState.h>

namespace igl::d3d12 {

std::shared_ptr<IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  return nullptr;
}

void RenderPipelineState::setRenderPipelineReflection(
    const IRenderPipelineReflection& /*renderPipelineReflection*/) {}

int RenderPipelineState::getIndexByName(const igl::NameHandle& /*name*/,
                                        ShaderStage /*stage*/) const {
  return -1;
}

int RenderPipelineState::getIndexByName(const std::string& /*name*/,
                                        ShaderStage /*stage*/) const {
  return -1;
}

} // namespace igl::d3d12
