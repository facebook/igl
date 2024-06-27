/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ComputePipelineState.h>

using namespace igl;

namespace igl::metal {

ComputePipelineState::ComputePipelineState(id<MTLComputePipelineState> value,
                                           MTLComputePipelineReflection* reflection) :
  value_(value), reflection_(reflection) {}

} // namespace igl::metal
