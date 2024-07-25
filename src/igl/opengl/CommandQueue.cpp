/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/CommandQueue.h>

#include <igl/Texture.h>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderPipelineState.h>

namespace igl::opengl {

void CommandQueue::setInitialContext(const std::shared_ptr<IContext>& context) {
  context_ = context;
}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& /*desc*/,
                                                                  Result* outResult) {
  //  IGL_ASSERT_MSG(
  //      activeCommandBuffers_ == 0,
  //      "OpenGL does not currently support creating multiple commandBuffers at the same time");
  if (context_ == nullptr) {
    Result::setResult(outResult, Result::Code::RuntimeError, "There is no context set");
    return nullptr;
  }

  auto commandBuffer = std::make_shared<CommandBuffer>(context_);
  activeCommandBuffers_++;
  Result::setOk(outResult);

  return commandBuffer;
}

SubmitHandle CommandQueue::submit(const ICommandBuffer& commandBuffer, bool /* endOfFrame */) {
  const auto& cb = static_cast<const CommandBuffer&>(commandBuffer);
  incrementDrawCount(cb.getCurrentDrawCount());

  activeCommandBuffers_--;

  return SubmitHandle{};
}

} // namespace igl::opengl
