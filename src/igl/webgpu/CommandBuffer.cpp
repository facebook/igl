/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/CommandBuffer.h>

#include <utility>
#include <igl/ComputeCommandEncoder.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

CommandBuffer::CommandBuffer(WebGPUContext& ctx, CommandBufferDesc desc) :
  ICommandBuffer(std::move(desc)),
  ctx_(ctx),
  serial_(ctx.getResourceTracker().openCommandBuffer()) {
  const WGPUCommandEncoderDescriptor encoderDesc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView(this->desc.debugName),
  };
  encoder_.reset(wgpuDeviceCreateCommandEncoder(ctx_.getDevice(), &encoderDesc));
}

CommandBuffer::~CommandBuffer() {
  if (!submitted_) {
    ctx_.getResourceTracker().closeCommandBuffer(serial_);
  }
}

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& /*renderPass*/,
    const std::shared_ptr<IFramebuffer>& /*framebuffer*/,
    const Dependencies& /*dependencies*/,
    Result* IGL_NULLABLE outResult) {
  Result::setResult(outResult, Result::Code::Unimplemented, "Render passes (WebGPU)");
  return nullptr;
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  IGL_LOG_ERROR_ONCE("Compute is not supported by the WebGPU backend yet\n");
  return nullptr;
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {}

void CommandBuffer::waitUntilScheduled() {}

void CommandBuffer::waitUntilCompleted() {
  if (!submitted_) {
    return;
  }
  const Result result = ctx_.waitForSubmittedWork();
  if (!result.isOk()) {
    IGL_LOG_ERROR("waitUntilCompleted(): %s\n", result.message.c_str());
  }
}

void CommandBuffer::pushDebugGroupLabel(const char* IGL_NONNULL label,
                                        const Color& /*color*/) const {
  wgpuCommandEncoderPushDebugGroup(encoder_.get(), toWGPUStringView(label));
}

void CommandBuffer::popDebugGroupLabel() const {
  wgpuCommandEncoderPopDebugGroup(encoder_.get());
}

void CommandBuffer::copyBuffer(IBuffer& src,
                               IBuffer& dst,
                               uint64_t srcOffset,
                               uint64_t dstOffset,
                               uint64_t size) {
  if (srcOffset % 4 != 0 || dstOffset % 4 != 0 || size % 4 != 0) {
    IGL_LOG_ERROR("copyBuffer(): offsets and size must be multiples of 4 on WebGPU\n");
    return;
  }
  if (submitted_ || size == 0) {
    return;
  }
  auto& srcBuffer = static_cast<Buffer&>(src);
  auto& dstBuffer = static_cast<Buffer&>(dst);
  wgpuCommandEncoderCopyBufferToBuffer(encoder_.get(),
                                       srcBuffer.getWGPUBuffer(),
                                       srcOffset,
                                       dstBuffer.getWGPUBuffer(),
                                       dstOffset,
                                       size);
  srcBuffer.recordUse(serial_, /*gpuWrite=*/false);
  dstBuffer.recordUse(serial_, /*gpuWrite=*/true);
}

void CommandBuffer::copyTextureToBuffer(ITexture& /*src*/,
                                        IBuffer& /*dst*/,
                                        uint64_t /*dstOffset*/,
                                        uint32_t /*level*/,
                                        uint32_t /*layer*/) {
  IGL_LOG_ERROR_ONCE("copyTextureToBuffer() is not supported by the WebGPU backend yet\n");
}

Result CommandBuffer::submit() {
  if (submitted_) {
    return Result(Result::Code::InvalidOperation, "The command buffer was already submitted");
  }
  submitted_ = true;
  const Handle<WGPUCommandBuffer> commands(wgpuCommandEncoderFinish(encoder_.get(), nullptr));
  encoder_ = nullptr;
  const WGPUCommandBuffer rawCommands = commands.get();
  wgpuQueueSubmit(ctx_.getQueue(), 1, &rawCommands);
  ctx_.getResourceTracker().closeCommandBuffer(serial_);
  return Result();
}

} // namespace igl::webgpu
