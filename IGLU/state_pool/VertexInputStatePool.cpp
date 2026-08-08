/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/state_pool/VertexInputStatePool.h>

#include <igl/Device.h>
#include <igl/Macros.h>

namespace iglu::state_pool {

std::shared_ptr<igl::IVertexInputState> VertexInputStatePool::createStateObject(
    igl::IDevice& dev,
    const igl::VertexInputStateDesc& desc,
    igl::Result* outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  return dev.createVertexInputState(desc, outResult);
}

} // namespace iglu::state_pool
