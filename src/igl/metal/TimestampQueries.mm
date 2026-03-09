/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/TimestampQueries.h>

#include <algorithm>

namespace igl::metal {

TimestampQueries::TimestampQueries(id<MTLCounterSampleBuffer> sampleBuffer,
                                   uint32_t maxTimestamps) :
  sampleBuffer_(sampleBuffer), maxTimestamps_(maxTimestamps) {
  // Each timing slot uses two internal counter samples (vertex-start + fragment-end).
  resolvedTimestamps_.reserve(maxTimestamps * 2);
}

TimestampQueries::~TimestampQueries() {
  sampleBuffer_ = nil;
}

uint32_t TimestampQueries::capacity() const {
  return maxTimestamps_;
}

uint32_t TimestampQueries::count() const {
  return std::min(currentIndex_.load(std::memory_order_relaxed), maxTimestamps_);
}

void TimestampQueries::reset() {
  generation_.fetch_add(1, std::memory_order_seq_cst);
  currentIndex_.store(0, std::memory_order_release);
  resolved_.store(false, std::memory_order_release);
}

bool TimestampQueries::resultsAvailable() const {
  return resolved_.load(std::memory_order_acquire);
}

uint64_t TimestampQueries::getElapsedNanos(uint32_t slotIndex) const {
  if (!resolved_.load(std::memory_order_acquire)) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(resolveMutex_);
  uint32_t startIdx = slotIndex * 2;
  uint32_t endIdx = slotIndex * 2 + 1;
  if (endIdx >= resolvedTimestamps_.size()) {
    return 0;
  }
  uint64_t start = resolvedTimestamps_[startIdx];
  uint64_t end = resolvedTimestamps_[endIdx];
  if (end > start) {
    return end - start;
  }
  return 0;
}

void TimestampQueries::resolveTimestamps(id<MTLCounterSampleBuffer> csb) {
  // Clamp to maxTimestamps_ to prevent out-of-bounds access on the counter sample buffer
  // in case sampleTimestamp was called more times than capacity allows (fetch_add overflow).
  uint32_t n = std::min(currentIndex_.load(std::memory_order_acquire), maxTimestamps_);
  if (n == 0) {
    return;
  }

  NSData* data = [csb resolveCounterRange:NSMakeRange(0, n)];
  if (!data || data.length < n * sizeof(MTLCounterResultTimestamp)) {
    return;
  }

  const MTLCounterResultTimestamp* timestamps =
      static_cast<const MTLCounterResultTimestamp*>(data.bytes);

  std::lock_guard<std::mutex> lock(resolveMutex_);
  resolvedTimestamps_.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    resolvedTimestamps_[i] = timestamps[i].timestamp;
  }
  resolved_.store(true, std::memory_order_release);
}

void TimestampQueries::sampleTimestampOnEncoder(id<MTLRenderCommandEncoder> encoder) {
  uint32_t idx = currentIndex_.fetch_add(1, std::memory_order_relaxed);
  if (idx >= maxTimestamps_) {
    return;
  }
  [encoder sampleCountersInBuffer:sampleBuffer_ atSampleIndex:idx withBarrier:YES];
}

void TimestampQueries::attachResolveHandler(id<MTLCommandBuffer> cmdBuffer,
                                            std::shared_ptr<TimestampQueries> queries) {
  if (!queries || !cmdBuffer) {
    return;
  }
  // Capture by value: shared_ptr extends lifetime, csb retains the ObjC object,
  // gen snapshots the current generation for staleness detection.
  id<MTLCounterSampleBuffer> csb = queries->sampleBuffer_;
  uint64_t gen = queries->generation_.load(std::memory_order_seq_cst);
  [cmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> /*cb*/) {
    @try {
      if (queries->generation_.load(std::memory_order_acquire) == gen) {
        queries->resolveTimestamps(csb);
      }
    } @catch (NSException*) {
      // GPU reset or counter sample buffer invalidated - silently skip.
    }
  }];
}

} // namespace igl::metal
