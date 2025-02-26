/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/CommandBuffer.h>

#import <Foundation/Foundation.h>

#import <Metal/Metal.h>
#include <igl/metal/ComputeCommandEncoder.h>
#include <igl/metal/RenderCommandEncoder.h>
#include <igl/metal/Texture.h>

namespace igl::metal {

CommandBuffer::CommandBuffer(igl::metal::Device& device, id<MTLCommandBuffer> value) :
  device_(device), value_(value) {}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return std::make_unique<ComputeCommandEncoder>(value_);
}

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& /*dependencies*/,
    Result* outResult) {
  return RenderCommandEncoder::create(shared_from_this(), renderPass, framebuffer, outResult);
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& surface) const {
  IGL_DEBUG_ASSERT(surface);
  if (!surface) {
    return;
  }
  const auto drawable = static_cast<Texture&>(*surface).getDrawable();
  if (drawable != nullptr) {
    [value_ presentDrawable:drawable];
  }
}

void CommandBuffer::pushDebugGroupLabel(const char* label, const igl::Color& /*color*/) const {
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  [value_ pushDebugGroup:[NSString stringWithUTF8String:label] ?: @""];
}

void CommandBuffer::popDebugGroupLabel() const {
  [value_ popDebugGroup];
}

void CommandBuffer::copyBuffer(IBuffer& src,
                               IBuffer& dst,
                               uint64_t srcOffset,
                               uint64_t dstOffset,
                               uint64_t size) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void CommandBuffer::waitUntilScheduled() {
  [value_ waitUntilScheduled];
}

void CommandBuffer::waitUntilCompleted() {
  [value_ waitUntilCompleted];
}

} // namespace igl::metal
