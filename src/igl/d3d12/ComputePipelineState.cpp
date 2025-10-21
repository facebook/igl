/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ComputePipelineState.h>

namespace igl::d3d12 {

std::shared_ptr<IComputePipelineReflection> ComputePipelineState::computePipelineReflection() {
  return nullptr;
}

void ComputePipelineState::setComputePipelineReflection(
    const IComputePipelineReflection& /*computePipelineReflection*/) {}

} // namespace igl::d3d12
