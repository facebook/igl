/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ComputeCommandEncoder.h>

#import <Foundation/Foundation.h>
#import <Metal/MTLComputePass.h>
#import <Metal/MTLTypes.h>
#include <igl/metal/Buffer.h>
#include <igl/metal/ComputePipelineState.h>
#include <igl/metal/SamplerState.h>
#include <igl/metal/Texture.h>
#include <igl/metal/TimestampQueries.h>

namespace igl::metal {

ComputeCommandEncoder::ComputeCommandEncoder(id<MTLCommandBuffer> buffer) {
  id<MTLComputeCommandEncoder> computeEncoder = [buffer computeCommandEncoder];
  encoder_ = computeEncoder;
}

ComputeCommandEncoder::ComputeCommandEncoder(id<MTLCommandBuffer> buffer,
                                             const ComputePassDesc& computePass) {
  // Wire MTLComputePassDescriptor::sampleBufferAttachments to the supplied TimestampQueries so the
  // resolved start/end samples land in the same slot pair (slot*2, slot*2+1) that the render-pass
  // path writes to. Consumers iterate slots uniformly across pass types via getElapsedNanos().
  // MTLComputePassDescriptor's sampleBufferAttachments require macOS 11 / iOS 14; older targets
  // fall back to a plain encoder (no timestamps) so existing callers still work.
  if (@available(macOS 11.0, iOS 14.0, *)) {
    if (computePass.timestampQuery.queries) {
      auto metalTsQueries =
          std::static_pointer_cast<TimestampQueries>(computePass.timestampQuery.queries);
      if (metalTsQueries && metalTsQueries->sampleBuffer_ != nil) {
        const uint32_t startSampleIdx =
            computePass.timestampQuery.slotIndex * TimestampQueries::kSamplesPerTimingSlot;
        const uint32_t endSampleIdx = startSampleIdx + 1;
        const uint32_t bufferSize =
            metalTsQueries->maxTimestamps_ * TimestampQueries::kSamplesPerTimingSlot;
        if (endSampleIdx < bufferSize) {
          MTLComputePassDescriptor* passDesc = [MTLComputePassDescriptor computePassDescriptor];
          passDesc.sampleBufferAttachments[0].sampleBuffer = metalTsQueries->sampleBuffer_;
          passDesc.sampleBufferAttachments[0].startOfEncoderSampleIndex = startSampleIdx;
          passDesc.sampleBufferAttachments[0].endOfEncoderSampleIndex = endSampleIdx;

          // Advance currentIndex_ so resolveTimestamps() knows how many samples to resolve.
          // Mirrors the render-pass path so a mixed render/compute frame produces one contiguous
          // resolve range, not two overlapping ones.
          const uint32_t requiredCount = endSampleIdx + 1;
          uint32_t current = metalTsQueries->currentIndex_.load(std::memory_order_relaxed);
          while (current < requiredCount) {
            if (metalTsQueries->currentIndex_.compare_exchange_weak(
                    current, requiredCount, std::memory_order_relaxed)) {
              break;
            }
          }

          encoder_ = [buffer computeCommandEncoderWithDescriptor:passDesc];
          return;
        }
      }
    }
  }
  encoder_ = [buffer computeCommandEncoder];
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

void ComputeCommandEncoder::dispatchThreadGroupsIndirect(IBuffer& indirectBuffer,
                                                         size_t indirectBufferOffset,
                                                         const Dimensions& threadgroupSize,
                                                         const Dependencies& /*dependencies*/) {
  IGL_DEBUG_ASSERT(encoder_);
  IGL_DEBUG_ASSERT((indirectBuffer.getBufferType() & BufferDesc::BufferTypeBits::Indirect) != 0,
                   "indirectBuffer must be created with BufferTypeBits::Indirect");
  IGL_DEBUG_ASSERT(indirectBufferOffset % 4 == 0, "indirectBufferOffset must be 4-byte aligned");

  const MTLSize tgs{
      .width = threadgroupSize.width,
      .height = threadgroupSize.height,
      .depth = threadgroupSize.depth,
  };

  auto& iglBuffer = static_cast<Buffer&>(indirectBuffer);
  [encoder_ dispatchThreadgroupsWithIndirectBuffer:iglBuffer.get()
                              indirectBufferOffset:indirectBufferOffset
                             threadsPerThreadgroup:tgs];
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

void ComputeCommandEncoder::bindImageTexture(uint32_t index,
                                             ITexture* texture,
                                             TextureFormat format) {
  (void)format;

  this->bindTexture(index, texture);
}

void ComputeCommandEncoder::bindSamplerState(uint32_t index, ISamplerState* samplerState) {
  IGL_DEBUG_ASSERT(encoder_);

  if (samplerState) {
    auto& iglSampler = static_cast<SamplerState&>(*samplerState);
    [encoder_ setSamplerState:iglSampler.get() atIndex:index];
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

void ComputeCommandEncoder::bindBytes(uint32_t index, const void* data, size_t length) {
  IGL_DEBUG_ASSERT(encoder_);
  if (data) {
    if (length > kMaxRecommendedBytes) {
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
