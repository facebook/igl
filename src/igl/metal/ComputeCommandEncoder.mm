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

namespace igl {
namespace metal {

ComputeCommandEncoder::ComputeCommandEncoder(id<MTLCommandBuffer> buffer) {
  id<MTLComputeCommandEncoder> computeEncoder = [buffer computeCommandEncoder];
  encoder_ = computeEncoder;
}

void ComputeCommandEncoder::endEncoding() {
  IGL_ASSERT(encoder_);
  [encoder_ endEncoding];
  encoder_ = nil;
}

void ComputeCommandEncoder::pushDebugGroupLabel(const std::string& label,
                                                const igl::Color& /*color*/) const {
  IGL_ASSERT(encoder_);
  IGL_ASSERT(!label.empty());
  [encoder_ pushDebugGroup:[NSString stringWithUTF8String:label.c_str()] ?: @""];
}

void ComputeCommandEncoder::insertDebugEventLabel(const std::string& label,
                                                  const igl::Color& /*color*/) const {
  IGL_ASSERT(encoder_);
  IGL_ASSERT(!label.empty());
  [encoder_ insertDebugSignpost:[NSString stringWithUTF8String:label.c_str()] ?: @""];
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  IGL_ASSERT(encoder_);
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
                                                 const Dimensions& threadgroupSize) {
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
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindTexture(size_t index, ITexture* texture) {
  IGL_ASSERT(encoder_);

  if (texture) {
    auto& iglTexture = static_cast<Texture&>(*texture);
    [encoder_ setTexture:iglTexture.get() atIndex:index];
  }
}

void ComputeCommandEncoder::bindBuffer(size_t index,
                                       const std::shared_ptr<IBuffer>& buffer,
                                       size_t offset) {
  IGL_ASSERT(encoder_);
  if (buffer) {
    auto& iglBuffer = static_cast<Buffer&>(*buffer);
    [encoder_ setBuffer:iglBuffer.get() offset:offset atIndex:index];
  }
}

void ComputeCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  IGL_ASSERT(encoder_);
  if (data) {
    if (length > MAX_RECOMMENDED_BYTES) {
      IGL_LOG_INFO(
          "It is recommended to use bindBuffer instead of bindBytes when binding > 4kb: %u",
          length);
    }
    [encoder_ setBytes:data length:length atIndex:index];
  }
}

void ComputeCommandEncoder::bindPushConstants(size_t /*offset*/,
                                              const void* /*data*/,
                                              size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

} // namespace metal
} // namespace igl
