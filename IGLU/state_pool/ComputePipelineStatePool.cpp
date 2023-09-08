/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/state_pool/ComputePipelineStatePool.h>

#include <igl/Device.h>

namespace iglu::state_pool {

std::shared_ptr<igl::IComputePipelineState> ComputePipelineStatePool::createStateObject(
    igl::IDevice& dev,
    const igl::ComputePipelineDesc& desc,
    igl::Result* outResult) {
  return dev.createComputePipeline(desc, outResult);
}

} // namespace iglu::state_pool
