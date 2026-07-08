/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/TimestampQueries.h>

#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <Foundation/NSRange.h>
#import <Metal/MTLCommandBuffer.h> // @donotremove
#import <Metal/MTLCounters.h>
#include <algorithm>
#include <limits>

namespace igl::metal {

TimestampQueries::TimestampQueries(id<MTLCounterSampleBuffer> sampleBuffer,
                                   uint32_t maxTimestamps) :
  sampleBuffer_(sampleBuffer), maxTimestamps_(maxTimestamps) {
  // Each timing slot uses two internal counter samples (vertex-start + fragment-end).
  resolvedTimestamps_.reserve(maxTimestamps * kSamplesPerTimingSlot);
}

TimestampQueries::~TimestampQueries() {
  sampleBuffer_ = nil;
}

uint32_t TimestampQueries::capacity() const {
  return maxTimestamps_;
}

uint32_t TimestampQueries::count() const {
  return std::min(currentIndex_.load(std::memory_order_relaxed) / kSamplesPerTimingSlot,
                  maxTimestamps_);
}

void TimestampQueries::reset() {
  generation_.fetch_add(1, std::memory_order_release);
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
  const uint32_t startIdx = slotIndex * kSamplesPerTimingSlot;
  const uint32_t endIdx = startIdx + 1;
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

uint64_t TimestampQueries::getStartNanos(uint32_t slotIndex) const {
  if (!resolved_.load(std::memory_order_acquire)) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(resolveMutex_);
  const uint32_t startIdx = slotIndex * kSamplesPerTimingSlot;
  if (startIdx >= resolvedTimestamps_.size()) {
    return 0;
  }
  return resolvedTimestamps_[startIdx];
}

uint64_t TimestampQueries::getEndNanos(uint32_t slotIndex) const {
  if (!resolved_.load(std::memory_order_acquire)) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(resolveMutex_);
  const uint32_t endIdx = slotIndex * kSamplesPerTimingSlot + 1;
  if (endIdx >= resolvedTimestamps_.size()) {
    return 0;
  }
  return resolvedTimestamps_[endIdx];
}

uint64_t TimestampQueries::getFrameElapsedNanos() const {
  if (!resolved_.load(std::memory_order_acquire)) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(resolveMutex_);
  // Walk every slot once and track the earliest vertex-start and the latest
  // fragment-end. `max(end) - min(start)` is the GPU wall-clock duration —
  // unlike summing per-slot deltas, it does NOT double-count the overlap
  // between consecutive passes that the GPU pipelines (pass N+1's vertex
  // stage starts while pass N's fragment stage is still running).
  uint64_t minStart = std::numeric_limits<uint64_t>::max();
  uint64_t maxEnd = 0;
  const size_t slots = resolvedTimestamps_.size() / kSamplesPerTimingSlot;
  for (size_t slot = 0; slot < slots; ++slot) {
    const uint64_t start = resolvedTimestamps_[slot * kSamplesPerTimingSlot];
    const uint64_t end = resolvedTimestamps_[slot * kSamplesPerTimingSlot + 1U];
    // Slots written but never paired with a valid end (e.g. abort()'d compute
    // paths on backends that still allocate a slot) leave end <= start —
    // skip them so the wall span isn't anchored to a stale zero.
    if (end <= start) {
      continue;
    }
    if (start < minStart) {
      minStart = start;
    }
    if (end > maxEnd) {
      maxEnd = end;
    }
  }
  if (maxEnd == 0 || minStart == std::numeric_limits<uint64_t>::max()) {
    return 0;
  }
  return maxEnd - minStart;
}

void TimestampQueries::resolveTimestamps(id<MTLCounterSampleBuffer> csb) {
  // Clamp to maxTimestamps_ * 2 because each timing slot uses two counter samples
  // (vertex-start and fragment-end), so the buffer has maxTimestamps_ * 2 entries.
  const uint32_t n = std::min(currentIndex_.load(std::memory_order_acquire),
                              maxTimestamps_ * kSamplesPerTimingSlot);
  if (n == 0) {
    return;
  }

  NSData* data = nil;
  @try {
    data = [csb resolveCounterRange:NSMakeRange(0, n)];
  } @catch (NSException*) {
    return; // GPU reset or counter sample buffer invalidated — skip this frame
  }
  if (!data || data.length < n * sizeof(MTLCounterResultTimestamp)) {
    return;
  }

  const auto* timestamps = static_cast<const MTLCounterResultTimestamp*>(data.bytes);

  std::lock_guard<std::mutex> lock(resolveMutex_);
  resolvedTimestamps_.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    resolvedTimestamps_[i] = timestamps[i].timestamp;
  }
  resolved_.store(true, std::memory_order_release);
}

void TimestampQueries::attachResolveHandler(id<MTLCommandBuffer> cmdBuffer,
                                            std::shared_ptr<TimestampQueries> queries) {
  if (!queries || !cmdBuffer) {
    return;
  }
  // Capture by value: shared_ptr extends lifetime, csb retains the ObjC object,
  // gen snapshots the current generation for staleness detection.
  id<MTLCounterSampleBuffer> csb = queries->sampleBuffer_;
  uint64_t gen = queries->generation_.load(std::memory_order_acquire);
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
