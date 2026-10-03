/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/TimestampQueries.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <igl/webgpu/TimestampQuerySet.h>

namespace igl::webgpu {

std::shared_ptr<TimestampQueries> TimestampQueries::create(WebGPUContext& ctx,
                                                           uint32_t maxTimestamps,
                                                           Result* IGL_NULLABLE outResult) {
  auto queries = TimestampQuerySet::create(ctx, 2 * maxTimestamps, outResult);
  if (!queries) {
    return nullptr;
  }
  return std::shared_ptr<TimestampQueries>(new TimestampQueries(std::move(queries), maxTimestamps));
}

TimestampQueries::TimestampQueries(std::unique_ptr<TimestampQuerySet> queries,
                                   uint32_t maxTimestamps) :
  queries_(std::move(queries)), maxTimestamps_(maxTimestamps) {}

TimestampQueries::~TimestampQueries() = default;

uint32_t TimestampQueries::capacity() const {
  return maxTimestamps_;
}

uint32_t TimestampQueries::count() const {
  return count_;
}

void TimestampQueries::reset() {
  count_ = 0;
  queries_->reset();
}

bool TimestampQueries::resultsAvailable() const {
  return queries_->pollAll();
}

std::optional<WGPUPassTimestampWrites> TimestampQueries::getPassTimestampWrites(
    uint32_t slotIndex) {
  if (slotIndex >= maxTimestamps_) {
    IGL_LOG_ERROR_ONCE("Timestamp query slot %u is out of range\n", slotIndex);
    return std::nullopt;
  }
  if (!queries_->getQuerySet()) {
    return std::nullopt;
  }
  count_ = std::max(count_, slotIndex + 1);
  WGPUPassTimestampWrites writes = WGPU_PASS_TIMESTAMP_WRITES_INIT;
  writes.querySet = queries_->getQuerySet();
  writes.beginningOfPassWriteIndex = 2 * slotIndex;
  writes.endOfPassWriteIndex = 2 * slotIndex + 1;
  return writes;
}

void TimestampQueries::encodeResolve(WGPUCommandEncoder IGL_NONNULL encoder) {
  queries_->encodeResolve(encoder, 2 * count_);
}

void TimestampQueries::onSubmitted() {
  queries_->mapAfterSubmit();
}

TimestampQueryResult TimestampQueries::getElapsedNanosResult(uint32_t slotIndex) const {
  if (!queries_->pollAll()) {
    return {};
  }
  const std::span<const uint64_t> timestamps = queries_->getTimestamps();
  const size_t end = 2 * size_t{slotIndex} + 1;
  if (end >= timestamps.size()) {
    return {};
  }
  const uint64_t startNanos = timestamps[end - 1];
  const uint64_t endNanos = timestamps[end];
  // Passes the GPU skipped (empty compute passes on Metal) leave both queries 0.
  if (startNanos == 0 || endNanos < startNanos) {
    return {};
  }
  return {.elapsedNanos = endNanos - startNanos, .valid = true};
}

uint64_t TimestampQueries::getElapsedNanos(uint32_t slotIndex) const {
  return getElapsedNanosResult(slotIndex).elapsedNanos;
}

uint64_t TimestampQueries::getStartNanos(uint32_t slotIndex) const {
  if (!queries_->pollAll()) {
    return 0;
  }
  const std::span<const uint64_t> timestamps = queries_->getTimestamps();
  const size_t index = 2 * size_t{slotIndex};
  return index + 1 < timestamps.size() ? timestamps[index] : 0;
}

uint64_t TimestampQueries::getEndNanos(uint32_t slotIndex) const {
  if (!queries_->pollAll()) {
    return 0;
  }
  const std::span<const uint64_t> timestamps = queries_->getTimestamps();
  const size_t index = 2 * size_t{slotIndex} + 1;
  return index < timestamps.size() ? timestamps[index] : 0;
}

uint64_t TimestampQueries::getFrameElapsedNanos() const {
  if (!queries_->pollAll()) {
    return 0;
  }
  const std::span<const uint64_t> timestamps = queries_->getTimestamps();
  uint64_t begin = std::numeric_limits<uint64_t>::max();
  uint64_t end = 0;
  for (size_t i = 0; i + 1 < timestamps.size(); i += 2) {
    if (timestamps[i] == 0 || timestamps[i + 1] < timestamps[i]) {
      continue;
    }
    begin = std::min(begin, timestamps[i]);
    end = std::max(end, timestamps[i + 1]);
  }
  return end > begin ? end - begin : 0;
}

} // namespace igl::webgpu
