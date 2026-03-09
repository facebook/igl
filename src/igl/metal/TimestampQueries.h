/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Metal/Metal.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <igl/TimestampQueries.h>

namespace igl::metal {

class CommandQueue;

class TimestampQueries : public ITimestampQueries {
 public:
  TimestampQueries(id<MTLCounterSampleBuffer> sampleBuffer, uint32_t maxTimestamps);
  ~TimestampQueries() override;

  uint32_t capacity() const override;
  uint32_t count() const override;
  void reset() override;
  bool resultsAvailable() const override;
  uint64_t getElapsedNanos(uint32_t slotIndex) const override;

  /// Called from RenderCommandEncoder::writeTimestamp() -- samples on active render encoder.
  /// No blit encoder created. Safe to call when a render encoder is active.
  void sampleTimestampOnEncoder(id<MTLRenderCommandEncoder> encoder);

  /// Called from CommandQueue completion handler -- resolves counter data
  void resolveTimestamps(id<MTLCounterSampleBuffer> csb);

  /// Attach a GPU completion handler to an externally-managed command buffer
  /// to resolve timestamps on GPU completion. Use this when the command buffer
  /// is NOT submitted via CommandQueue::submit().
  /// @param cmdBuffer The Metal command buffer to attach the handler to.
  /// @param queries Owning shared_ptr captured in the completion block to
  ///        prevent use-after-free (mirrors CommandQueue::submit pattern).
  static void attachResolveHandler(id<MTLCommandBuffer> cmdBuffer,
                                   std::shared_ptr<TimestampQueries> queries);

 private:
  id<MTLCounterSampleBuffer> sampleBuffer_ = nil;
  uint32_t maxTimestamps_ = 0;
  std::atomic<uint32_t> currentIndex_{0};
  std::atomic<uint64_t> generation_{0};
  std::vector<uint64_t> resolvedTimestamps_;
  std::atomic<bool> resolved_{false};
  mutable std::mutex resolveMutex_;

  friend class CommandQueue;
  friend class RenderCommandEncoder;
};

} // namespace igl::metal
