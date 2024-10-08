/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ComputeCommandEncoder.h>

#import <Foundation/Foundation.h>

#import <Metal/Metal.h>
#include <igl/metal/Buffer.h>
#include <igl/metal/ComputePipelineState.h>
#include <igl/metal/Framebuffer.h>
#include <igl/metal/SamplerState.h>
#include <igl/metal/Texture.h>

namespace igl::metal {

ComputeCommandEncoder::ComputeCommandEncoder(id<MTLCommandBuffer> buffer) {
  id<MTLComputeCommandEncoder> computeEncoder = [buffer computeCommandEncoder];
  encoder_ = computeEncoder;
}

void ComputeCommandEncoder::endEncoding() {
  IGL_DEBUG_ASSERT(encoder_);
  [encoder_ endEncoding];
  encoder_ = nil;
}

void ComputeCommandEncoder::pushDebugGroupLabel(const char* label,
                                                const igl::Color& /*color*/) const {
  IGL_DEBUG_ASSERT(encoder_);
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  [encoder_ pushDebugGroup:[NSString stringWithUTF8String:label] ?: @""];
}

void ComputeCommandEncoder::insertDebugEventLabel(const char* label,
                                                  const igl::Color& /*color*/) const {
  IGL_DEBUG_ASSERT(encoder_);
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  [encoder_ insertDebugSignpost:[NSString stringWithUTF8String:label] ?: @""];
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  IGL_DEBUG_ASSERT(encoder_);
  [encoder_ popDebugGroup];
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  if (pipelineState) {
    auto& iglPipelineState = static_cast<ComputePipelineState&>(*pipelineState);
    [encoder_ setComputePipelineState:iglPipelineState.get()];
  }
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& threadgroupSize,
                                                 const Dependencies& /*dependencies*/) {
  MTLSize tgc;
  tgc.width = threadgroupCount.width;
  tgc.height = threadgroupCount.height;
  tgc.depth = threadgroupCount.depth;

  MTLSize tgs;
  tgs.width = threadgroupSize.width;
  tgs.height = threadgroupSize.height;
  tgs.depth = threadgroupSize.depth;
  [encoder_ dispatchThreadgroups:tgc threadsPerThreadgroup:tgs];
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // DO NOT IMPLEMENT!
  // This is only for backends that MUST use single uniforms in some situations.
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindTexture(uint32_t index, ITexture* texture) {
  IGL_DEBUG_ASSERT(encoder_);

  if (texture) {
    auto& iglTexture = static_cast<Texture&>(*texture);
    [encoder_ setTexture:iglTexture.get() atIndex:index];
  }
}

void ComputeCommandEncoder::bindBuffer(uint32_t index,
                                       IBuffer* buffer,
                                       size_t offset,
                                       size_t bufferSize) {
  (void)bufferSize;

  IGL_DEBUG_ASSERT(encoder_);
  if (buffer) {
    auto& iglBuffer = static_cast<Buffer&>(*buffer);
    [encoder_ setBuffer:iglBuffer.get() offset:offset atIndex:index];
  }
}

void ComputeCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  IGL_DEBUG_ASSERT(encoder_);
  if (data) {
    if (length > MAX_RECOMMENDED_BYTES) {
      IGL_LOG_INFO(
          "It is recommended to use bindBuffer instead of bindBytes when binding > 4kb: %u",
          length);
    }
    [encoder_ setBytes:data length:length atIndex:index];
  }
}

void ComputeCommandEncoder::bindPushConstants(const void* /*data*/,
                                              size_t /*length*/,
                                              size_t /*offset*/) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

} // namespace igl::metal
