/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Common.h>
#include <igl/Timer.h>

namespace igl {
class IDevice;
} // namespace igl

namespace iglu::gpu_benchmark {

enum class TimerKind : uint8_t {
  /// An igl::ITimer attached to each command buffer.
  GpuTimer,
  /// The device has no GPU timers, so callers time blocks of work with a steady clock.
  CpuWallClock,
};

struct TimedCommandBuffer {
  std::shared_ptr<igl::ICommandBuffer> commandBuffer;
  /// Null when the command buffer is not GPU timed.
  std::shared_ptr<igl::ITimer> timer;
};

/// Creates command buffers whose GPU execution time can be read back.
///
/// A timer measures a single command buffer. On OpenGL it starts measuring when it is created and
/// cannot be reused, so createCommandBuffer() creates a new timer right before each command buffer.
/// That is the portable pattern on every backend.
///
/// Without GPU timers (igl::DeviceFeatures::Timers), or once the device fails to create one, the
/// kind is TimerKind::CpuWallClock: createCommandBuffer() creates a plain command buffer and
/// getElapsedNanos() returns std::nullopt, so callers time blocks of work with a steady clock.
///
/// Call everything from the thread that owns the device, which on OpenGL is the thread of the
/// context. On OpenGL:
///  - The measurement runs from the creation of the timer to the submit of the command buffer. Do
///    all CPU preparation first and create the timed command buffer right before encoding.
///  - Only one timed command buffer may be recording at a time, and it must be submitted: its
///    GL_TIME_ELAPSED query stays open until then.
///  - Timers cannot be combined with igl::RenderPassDesc::timestampQuery, because both use
///    GL_TIME_ELAPSED.
///
/// The device must outlive the GpuFrameTimer.
class GpuFrameTimer {
 public:
  explicit GpuFrameTimer(const igl::IDevice& device);

  /// TimerKind::GpuTimer if the device has GPU timers, until createCommandBuffer() fails to create
  /// one.
  [[nodiscard]] TimerKind getKind() const noexcept;

  /// Creates a command buffer on `queue`, with a new timer attached for TimerKind::GpuTimer. If
  /// the device cannot create the timer, the reason is logged once, the kind becomes
  /// TimerKind::CpuWallClock for good, and the command buffer is created without a timer.
  [[nodiscard]] TimedCommandBuffer createCommandBuffer(igl::ICommandQueue& queue,
                                                       const std::string& debugName,
                                                       igl::Result* IGL_NULLABLE outResult) const;

  /// Elapsed GPU time of `buffer` in nanoseconds. Call it after
  /// igl::ICommandBuffer::waitUntilCompleted() on the buffer. The result can arrive later than the
  /// wait returns (Metal fills it from a completion handler), so this polls
  /// igl::ITimer::resultsAvailable() every 100 microseconds until `timeout` has passed. It polls at
  /// least once, so a zero `timeout` does not wait.
  ///
  /// Returns std::nullopt if `buffer` has no timer, no result became available in time, or the
  /// timer measured 0.
  [[nodiscard]] std::optional<uint64_t> getElapsedNanos(
      const TimedCommandBuffer& buffer,
      std::chrono::microseconds timeout = std::chrono::milliseconds(20)) const;

  /// Like getElapsedNanos(), but checks igl::ITimer::resultsAvailable() once and never waits.
  [[nodiscard]] std::optional<uint64_t> tryGetElapsedNanos(const TimedCommandBuffer& buffer) const;

 private:
  const igl::IDevice& device_;
  mutable TimerKind kind_;
};

} // namespace iglu::gpu_benchmark
