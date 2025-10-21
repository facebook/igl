/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ComputeCommandEncoder.h>

namespace igl::d3d12 {

void ComputeCommandEncoder::endEncoding() {}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& /*pipelineState*/) {}
void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& /*threadgroupCount*/,
                                                 const Dimensions& /*threadgroupSize*/,
                                                 const Dependencies& /*dependencies*/) {}
void ComputeCommandEncoder::bindPushConstants(const void* /*data*/,
                                              size_t /*length*/,
                                              size_t /*offset*/) {}
void ComputeCommandEncoder::bindTexture(uint32_t /*index*/, ITexture* /*texture*/) {}

} // namespace igl::d3d12
