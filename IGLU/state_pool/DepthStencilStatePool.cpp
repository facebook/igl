/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/state_pool/DepthStencilStatePool.h>

#include <igl/Device.h>
#include <igl/Macros.h>

namespace iglu::state_pool {

std::shared_ptr<igl::IDepthStencilState> DepthStencilStatePool::createStateObject(
    igl::IDevice& dev,
    const igl::DepthStencilStateDesc& desc,
    igl::Result* outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  return dev.createDepthStencilState(desc, outResult);
}

} // namespace iglu::state_pool
