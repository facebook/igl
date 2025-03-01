/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/CommandBuffer.h>

#include <igl/opengl/Buffer.h>
#include <igl/opengl/ComputeCommandEncoder.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderCommandEncoder.h>

namespace igl::opengl {

CommandBuffer::CommandBuffer(std::shared_ptr<IContext> context) : context_(std::move(context)) {}

CommandBuffer::~CommandBuffer() = default;

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& dependencies,
    Result* outResult) {
  return RenderCommandEncoder::create(
      shared_from_this(), renderPass, framebuffer, dependencies, outResult);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return std::make_unique<ComputeCommandEncoder>(shared_from_this()->getContext());
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& surface) const {
  context_->present(surface);
}

void CommandBuffer::waitUntilScheduled() {
  context_->flush();
}

void CommandBuffer::waitUntilCompleted() {
  context_->finish();
}

void CommandBuffer::pushDebugGroupLabel(const char* label, const igl::Color& /*color*/) const {
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    getContext().pushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
  } else {
    IGL_LOG_ERROR_ONCE("CommandBuffer::pushDebugGroupLabel not supported in this context!\n");
  }
}

void CommandBuffer::popDebugGroupLabel() const {
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugMessage)) {
    getContext().popDebugGroup();
  } else {
    IGL_LOG_ERROR_ONCE("CommandBuffer::popDebugGroupLabel not supported in this context!\n");
  }
}

void CommandBuffer::copyBuffer(IBuffer& src,
                               IBuffer& dst,
                               uint64_t srcOffset,
                               uint64_t dstOffset,
                               uint64_t size) {
  IContext& ctx = getContext();

  if (!ctx.deviceFeatures().hasFeature(igl::DeviceFeatures::CopyBuffer)) {
    IGL_LOG_ERROR_ONCE("CommandBuffer::copyBuffer() not supported in this context!\n");
    return;
  }

  auto& srcBuffer = static_cast<ArrayBuffer&>(src);
  auto& dstBuffer = static_cast<ArrayBuffer&>(dst);

  ctx.bindBuffer(GL_COPY_READ_BUFFER, srcBuffer.getId());
  ctx.bindBuffer(GL_COPY_WRITE_BUFFER, dstBuffer.getId());
  ctx.copyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcOffset, dstOffset, size);
  ctx.bindBuffer(GL_COPY_READ_BUFFER, 0);
  ctx.bindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

IContext& CommandBuffer::getContext() const {
  return *context_;
}

} // namespace igl::opengl
