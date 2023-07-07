/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/CommandBuffer.h>

#include <igl/opengl/ComputeCommandEncoder.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderCommandEncoder.h>

namespace igl {
namespace opengl {

CommandBuffer::CommandBuffer(std::shared_ptr<IContext> context) : context_(std::move(context)) {}

CommandBuffer::~CommandBuffer() = default;

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    std::shared_ptr<IFramebuffer> framebuffer,
    Result* outResult) {
  return RenderCommandEncoder::create(shared_from_this(), renderPass, framebuffer, outResult);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return std::make_unique<ComputeCommandEncoder>(shared_from_this()->getContext());
}

void CommandBuffer::present(std::shared_ptr<ITexture> surface) const {
  context_->present(surface);
}

void CommandBuffer::waitUntilScheduled() {
  context_->flush();
}

void CommandBuffer::waitUntilCompleted() {
  context_->finish();
}

void CommandBuffer::pushDebugGroupLabel(const std::string& label,
                                        const igl::Color& /*color*/) const {
  IGL_ASSERT(!label.empty());
  getContext().pushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, label.length(), label.c_str());
}

void CommandBuffer::popDebugGroupLabel() const {
  getContext().popDebugGroup();
}

IContext& CommandBuffer::getContext() const {
  return *context_;
}

} // namespace opengl
} // namespace igl
