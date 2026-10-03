/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Timer.h>

#include <algorithm>
#include <limits>
#include <span>
#include <utility>
#include <igl/webgpu/TimestampQuerySet.h>

namespace igl::webgpu {

std::shared_ptr<Timer> Timer::create(WebGPUContext& ctx, Result* IGL_NULLABLE outResult) {
  auto queries = TimestampQuerySet::create(ctx, 2 * kMaxPasses, outResult);
  if (!queries) {
    return nullptr;
  }
  return std::shared_ptr<Timer>(new Timer(std::move(queries)));
}

Timer::Timer(std::unique_ptr<TimestampQuerySet> queries) : queries_(std::move(queries)) {}

Timer::~Timer() = default;

void Timer::begin() {
  numPasses_ = 0;
  // The previous command buffer's result stays readable until this one's completes, as on Metal.
  queries_->restart();
}

std::optional<WGPUPassTimestampWrites> Timer::nextPass() {
  if (!queries_->getQuerySet()) {
    return std::nullopt;
  }
  if (numPasses_ == kMaxPasses) {
    IGL_LOG_INFO_ONCE("WebGPU timers measure the first %u passes of a command buffer\n",
                      kMaxPasses);
    return std::nullopt;
  }
  const uint32_t pass = numPasses_++;
  WGPUPassTimestampWrites writes = WGPU_PASS_TIMESTAMP_WRITES_INIT;
  writes.querySet = queries_->getQuerySet();
  writes.beginningOfPassWriteIndex = 2 * pass;
  writes.endOfPassWriteIndex = 2 * pass + 1;
  return writes;
}

void Timer::encodeResolve(WGPUCommandEncoder IGL_NONNULL encoder) {
  queries_->encodeResolve(encoder, 2 * numPasses_);
}

void Timer::onSubmitted() {
  queries_->mapAfterSubmit();
}

bool Timer::resultsAvailable() const {
  return queries_->poll();
}

uint64_t Timer::getElapsedTimeNanos() const {
  if (!queries_->poll()) {
    return 0;
  }
  const std::span<const uint64_t> timestamps = queries_->getTimestamps();
  uint64_t begin = std::numeric_limits<uint64_t>::max();
  uint64_t end = 0;
  // Passes the GPU skipped (empty compute passes on Metal) leave both queries 0.
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
