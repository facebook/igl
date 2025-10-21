/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandQueue.h>

namespace igl::d3d12 {

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& /*desc*/,
                                                                   Result* IGL_NULLABLE
                                                                       outResult) {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 CommandBuffer not yet implemented");
  return nullptr;
}

SubmitHandle CommandQueue::submit(const ICommandBuffer& /*commandBuffer*/, bool /*endOfFrame*/) {
  return 0;
}

} // namespace igl::d3d12
