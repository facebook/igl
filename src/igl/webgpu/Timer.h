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
#include <igl/Timer.h>

namespace igl::webgpu {

class TimestampQuerySet;
class WebGPUContext;

/// @brief Implements the igl::ITimer interface with pass timestamp writes.
///
/// The timer measures the command buffer it is attached to (CommandBufferDesc::timer) from the
/// beginning of its first render or compute pass to the end of its last one. The first
/// kMaxPasses passes are timed. Command buffers without passes have no result.
class Timer final : public ITimer {
 public:
  static constexpr uint32_t kMaxPasses = 64;

  [[nodiscard]] static std::shared_ptr<Timer> create(WebGPUContext& ctx,
                                                     Result* IGL_NULLABLE outResult);
  ~Timer() override;

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;
  Timer(Timer&&) = delete;
  Timer& operator=(Timer&&) = delete;

  [[nodiscard]] uint64_t getElapsedTimeNanos() const override;
  [[nodiscard]] bool resultsAvailable() const override;

  /// Starts timing a new command buffer.
  void begin();
  /// Timestamp writes for the next pass, or std::nullopt once kMaxPasses passes are timed.
  [[nodiscard]] std::optional<WGPUPassTimestampWrites> nextPass();
  /// Resolves the timed passes before the command buffer is finished.
  void encodeResolve(WGPUCommandEncoder IGL_NONNULL encoder);
  void onSubmitted();

 private:
  explicit Timer(std::unique_ptr<TimestampQuerySet> queries);

  std::unique_ptr<TimestampQuerySet> queries_;
  uint32_t numPasses_ = 0;
};

} // namespace igl::webgpu
