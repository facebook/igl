/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/gpu_benchmark/GpuFrameTimer.h>

#include <thread>
#include <igl/Device.h>

namespace iglu::gpu_benchmark {

namespace {

constexpr std::chrono::microseconds kPollInterval{100};

[[nodiscard]] std::optional<uint64_t> readElapsedNanos(const igl::ITimer& timer) {
  const uint64_t nanos = timer.getElapsedTimeNanos();
  return nanos != 0 ? std::optional<uint64_t>{nanos} : std::nullopt;
}

} // namespace

GpuFrameTimer::GpuFrameTimer(const igl::IDevice& device) :
  device_(device),
  kind_(device.hasFeature(igl::DeviceFeatures::Timers) ? TimerKind::GpuTimer
                                                       : TimerKind::CpuWallClock) {}

TimerKind GpuFrameTimer::getKind() const noexcept {
  return kind_;
}

TimedCommandBuffer GpuFrameTimer::createCommandBuffer(igl::ICommandQueue& queue,
                                                      const std::string& debugName,
                                                      igl::Result* IGL_NULLABLE outResult) const {
  TimedCommandBuffer timed;
  if (kind_ == TimerKind::GpuTimer) {
    igl::Result timerResult;
    timed.timer = device_.createTimer(&timerResult);
    if (!timerResult.isOk() || !timed.timer) {
      IGLLog(IGLLogWarning,
             "[gpu_benchmark] Cannot create a GPU timer (%s), timing with the CPU wall clock\n",
             timerResult.isOk() ? "no timer returned" : timerResult.message.c_str());
      timed.timer.reset();
      kind_ = TimerKind::CpuWallClock;
    }
  }
  timed.commandBuffer =
      queue.createCommandBuffer({.debugName = debugName, .timer = timed.timer}, outResult);
  return timed;
}

std::optional<uint64_t> GpuFrameTimer::getElapsedNanos(const TimedCommandBuffer& buffer,
                                                       std::chrono::microseconds timeout) const {
  if (!buffer.timer) {
    return std::nullopt;
  }
  const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
  while (!buffer.timer->resultsAvailable()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return std::nullopt;
    }
    // NOLINTNEXTLINE(facebook-hte-BadCall-sleep_for)
    std::this_thread::sleep_for(kPollInterval);
  }
  return readElapsedNanos(*buffer.timer);
}

std::optional<uint64_t> GpuFrameTimer::tryGetElapsedNanos(const TimedCommandBuffer& buffer) const {
  if (!buffer.timer || !buffer.timer->resultsAvailable()) {
    return std::nullopt;
  }
  return readElapsedNanos(*buffer.timer);
}

} // namespace iglu::gpu_benchmark
