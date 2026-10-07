/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <webgpu/webgpu.h>
#include <igl/Common.h>
#include <igl/TimestampQueries.h>

namespace igl::webgpu {

class TimestampQuerySet;
class WebGPUContext;

/// @brief Implements the igl::ITimestampQueries interface: timing slot i of a render or compute
/// pass writes queries 2i (beginning of the pass) and 2i+1 (end of the pass).
///
/// Slots are resolved when a command buffer that wrote them is submitted and become readable
/// asynchronously; resultsAvailable() processes WebGPU events.
class TimestampQueries final : public ITimestampQueries {
 public:
  [[nodiscard]] static std::shared_ptr<TimestampQueries> create(WebGPUContext& ctx,
                                                                uint32_t maxTimestamps,
                                                                Result* IGL_NULLABLE outResult);
  ~TimestampQueries() override;

  TimestampQueries(const TimestampQueries&) = delete;
  TimestampQueries& operator=(const TimestampQueries&) = delete;
  TimestampQueries(TimestampQueries&&) = delete;
  TimestampQueries& operator=(TimestampQueries&&) = delete;

  [[nodiscard]] uint32_t capacity() const override;
  [[nodiscard]] uint32_t count() const override;
  void reset() override;
  [[nodiscard]] bool resultsAvailable() const override;
  [[nodiscard]] uint64_t getElapsedNanos(uint32_t slotIndex) const override;
  [[nodiscard]] TimestampQueryResult getElapsedNanosResult(uint32_t slotIndex) const override;
  [[nodiscard]] uint64_t getStartNanos(uint32_t slotIndex) const override;
  [[nodiscard]] uint64_t getEndNanos(uint32_t slotIndex) const override;
  [[nodiscard]] uint64_t getFrameElapsedNanos() const override;
  [[nodiscard]] bool supportsComputePassTimestamps() const override {
    return true;
  }

  /// Timestamp writes of `slotIndex`, or std::nullopt when it is out of range.
  [[nodiscard]] std::optional<WGPUPassTimestampWrites> getPassTimestampWrites(uint32_t slotIndex);
  /// Resolves the recorded slots before a command buffer that wrote them is finished.
  void encodeResolve(WGPUCommandEncoder IGL_NONNULL encoder);
  void onSubmitted();

 private:
  TimestampQueries(std::unique_ptr<TimestampQuerySet> queries, uint32_t maxTimestamps);

  std::unique_ptr<TimestampQuerySet> queries_;
  const uint32_t maxTimestamps_;
  uint32_t count_ = 0;
};

} // namespace igl::webgpu
