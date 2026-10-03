/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/CommandQueue.h>

#include <igl/webgpu/CommandBuffer.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                  Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (ctx_.isDeviceLost()) {
    Result::setResult(outResult, Result::Code::DeviceLost, "The WebGPU device was lost");
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_shared<CommandBuffer>(ctx_, desc);
}

SubmitHandle CommandQueue::submit(const ICommandBuffer& commandBuffer, bool /*endOfFrame*/) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_SUBMIT);
  // IGL's submit() takes the command buffer as const, but submission consumes it.
  auto& cmdBuffer = const_cast<CommandBuffer&>(static_cast<const CommandBuffer&>(commandBuffer));
  const Result result = cmdBuffer.submit();
  if (!result.isOk()) {
    IGL_LOG_ERROR("submit(): %s\n", result.message.c_str());
    return 0;
  }
  incrementDrawCount(commandBuffer.getCurrentDrawCount());
  ctx_.processEvents();
  return cmdBuffer.getSerial();
}

} // namespace igl::webgpu
