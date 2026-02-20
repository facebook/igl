/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/RenderSession.h>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/Common.h>

namespace igl::shell {

RenderSession::RenderSession(std::shared_ptr<Platform> platform) :
  platform_(std::move(platform)), appParams_(std::make_shared<AppParams>()) {}

void RenderSession::updateDisplayScale(float scale) noexcept {
  platform_->getDisplayContext().scale = scale;
}

float RenderSession::pixelsPerPoint() const noexcept {
  return platform_->getDisplayContext().pixelsPerPoint;
}

void RenderSession::setPixelsPerPoint(float scale) noexcept {
  platform_->getDisplayContext().pixelsPerPoint = scale;
}

const ShellParams& RenderSession::shellParams() const noexcept {
  static const ShellParams kSentinelParams = {};
  return shellParams_ ? *shellParams_ : kSentinelParams;
}

const AppParams& RenderSession::appParams() const noexcept {
  return *appParams_;
}

AppParams& RenderSession::appParamsRef() noexcept {
  return *appParams_;
}

Platform& RenderSession::getPlatform() noexcept {
  return *platform_;
}
const Platform& RenderSession::getPlatform() const noexcept {
  return *platform_;
}

const std::shared_ptr<Platform>& RenderSession::platform() const noexcept {
  return platform_;
}

float RenderSession::getDeltaSeconds() noexcept {
  const double newTime = getSeconds();
  const float deltaSeconds = float(newTime - lastTime_);
  lastTime_ = newTime;
  return deltaSeconds;
}

double RenderSession::getSeconds() noexcept {
  return std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

void RenderSession::setPreferredClearColor(const igl::Color& color) noexcept {
  preferredClearColor_ = color;
}

Color RenderSession::getPreferredClearColor() noexcept {
  return preferredClearColor_.has_value() ? preferredClearColor_.value()
                                          : platform()->getDevice().backendDebugColor();
}

void RenderSession::initBenchmarkTracker() noexcept {
  if (!shellParams_ || !shellParams_->benchmarkParams.has_value()) {
    return;
  }

  const auto& benchmarkParams = shellParams_->benchmarkParams.value();
  benchmarkTracker_ = std::make_unique<BenchmarkTracker>(benchmarkParams.renderTimeBufferSize);
  benchmarkTracker_->setBenchmarkDuration(benchmarkParams.benchmarkDurationMs);
  benchmarkTracker_->setReportInterval(benchmarkParams.reportIntervalMs);
  benchmarkTracker_->setHiccupMultiplier(benchmarkParams.hiccupMultiplier);

  IGL_LOG_INFO("[IGL Benchmark] Benchmark tracking initialized\n");
  IGL_LOG_INFO("[IGL Benchmark]   Duration: %zu ms (%.1f minutes)\n",
               benchmarkParams.benchmarkDurationMs,
               benchmarkParams.benchmarkDurationMs / 60000.0);
  IGL_LOG_INFO("[IGL Benchmark]   Report Interval: %zu ms (%.1f seconds)\n",
               benchmarkParams.reportIntervalMs,
               benchmarkParams.reportIntervalMs / 1000.0);
  IGL_LOG_INFO("[IGL Benchmark]   Hiccup Multiplier: %.1f\n", benchmarkParams.hiccupMultiplier);
  IGL_LOG_INFO("[IGL Benchmark]   Buffer Size: %zu samples\n",
               benchmarkParams.renderTimeBufferSize);
}

void RenderSession::recordBenchmarkFrame(double renderTimeMs) noexcept {
  if (!benchmarkTracker_) {
    return;
  }

  benchmarkTracker_->recordRenderTime(renderTimeMs);

  // Log hiccups immediately when detected
  if (benchmarkTracker_->wasLastFrameHiccup()) {
    IGL_LOG_INFO("[IGL Benchmark] *** HICCUP DETECTED *** Frame time: %.2f ms (avg: %.2f ms)\n",
                 renderTimeMs,
                 benchmarkTracker_->getRunningAverageMs());
  }
}

void RenderSession::checkBenchmarkPeriodicReport() noexcept {
  if (!benchmarkTracker_) {
    return;
  }

  if (benchmarkTracker_->shouldGeneratePeriodicReport()) {
    const auto stats = benchmarkTracker_->computeStats();
    const double elapsedMin = benchmarkTracker_->getElapsedTimeMs() / 60000.0;

    IGL_LOG_INFO("[IGL Benchmark] === Periodic Report (%.1f min elapsed) ===\n", elapsedMin);
    IGL_LOG_INFO("[IGL Benchmark] FPS: avg=%.1f, min=%.1f, max=%.1f\n",
                 stats.avgFps,
                 stats.minFps,
                 stats.maxFps);
    IGL_LOG_INFO("[IGL Benchmark] Frame time (ms): avg=%.2f, min=%.2f, max=%.2f\n",
                 stats.avgRenderTimeMs,
                 stats.minRenderTimeMs,
                 stats.maxRenderTimeMs);
    IGL_LOG_INFO("[IGL Benchmark] Total frames: %zu\n", stats.totalSamples);

    // Suppress unused variable warnings when logging is disabled
    (void)stats;
    (void)elapsedMin;

    benchmarkTracker_->markPeriodicReportGenerated();
  }
}

bool RenderSession::isBenchmarkExpired() const noexcept {
  if (!benchmarkTracker_) {
    return false;
  }
  return benchmarkTracker_->hasBenchmarkExpired();
}

void RenderSession::logFinalBenchmarkReport(bool wasTimeout) noexcept {
  if (!benchmarkTracker_) {
    return;
  }

  // Log the final report line by line to ensure it appears in logcat
  const auto stats = benchmarkTracker_->computeStats();
  const double elapsedSec = benchmarkTracker_->getElapsedTimeMs() / 1000.0;
  const double elapsedMin = elapsedSec / 60.0;

  IGL_LOG_INFO("[IGL Benchmark] ========== FINAL BENCHMARK REPORT ==========\n");
  if (wasTimeout) {
    IGL_LOG_INFO("[IGL Benchmark] Status: COMPLETED SUCCESSFULLY (benchmark timeout reached)\n");
  } else {
    IGL_LOG_INFO("[IGL Benchmark] Status: COMPLETED (application terminated normally)\n");
  }
  IGL_LOG_INFO("[IGL Benchmark] Duration: %.1f minutes (%.1f seconds)\n", elapsedMin, elapsedSec);
  IGL_LOG_INFO("[IGL Benchmark] Total Frames: %zu\n", stats.totalSamples);
  IGL_LOG_INFO("[IGL Benchmark] ---------- FPS Statistics ----------\n");
  IGL_LOG_INFO("[IGL Benchmark] Average FPS: %.1f\n", stats.avgFps);
  IGL_LOG_INFO("[IGL Benchmark] Minimum FPS: %.1f\n", stats.minFps);
  IGL_LOG_INFO("[IGL Benchmark] Maximum FPS: %.1f\n", stats.maxFps);
  IGL_LOG_INFO("[IGL Benchmark] ---------- Frame Time Statistics ----------\n");
  IGL_LOG_INFO("[IGL Benchmark] Average: %.2f ms\n", stats.avgRenderTimeMs);
  IGL_LOG_INFO("[IGL Benchmark] Minimum: %.2f ms\n", stats.minRenderTimeMs);
  IGL_LOG_INFO("[IGL Benchmark] Maximum: %.2f ms\n", stats.maxRenderTimeMs);
  IGL_LOG_INFO("[IGL Benchmark] Overflow Records: %zu\n",
               benchmarkTracker_->getOverflowRecordCount());
  IGL_LOG_INFO("[IGL Benchmark] ===============================================\n");

  // Suppress unused variable warnings when logging is disabled
  (void)stats;
  (void)elapsedSec;
  (void)elapsedMin;
  (void)wasTimeout;
}

void RenderSession::runUpdate(SurfaceTextures surfaceTextures) noexcept {
  // Check if frozen (frame gate)
  if (frozen_) {
    return;
  }

  // Initialize benchmark tracker on first call if not already done
  if (shellParams_ && shellParams_->benchmarkParams.has_value() && !benchmarkTracker_) {
    initBenchmarkTracker();
  }

  // Debug: Log once if benchmark params are missing
  if (!loggedMissingParams_ && !benchmarkTracker_) {
    if (!shellParams_) {
      IGL_LOG_INFO("[IGL Benchmark] WARNING: shellParams_ is null, benchmark tracking disabled\n");
    } else if (!shellParams_->benchmarkParams.has_value()) {
      IGL_LOG_INFO(
          "[IGL Benchmark] WARNING: benchmarkParams not set, benchmark tracking disabled\n");
      IGL_LOG_INFO(
          "[IGL Benchmark] Use --benchmark flag or set "
          "debug.iglshell.renderSession.benchmark=true\n");
    }
    loggedMissingParams_ = true;
  }

  // Check freeze-at-frame gate
  if (shellParams_ && shellParams_->freezeAtFrame != ~0u &&
      frameCount_ >= shellParams_->freezeAtFrame) {
    frozen_ = true;
    IGL_LOG_INFO("[IGL Shell] Frozen at frame %u\n", shellParams_->freezeAtFrame);
    return;
  }

  // Measure render time for benchmarking
  const double startTime = getSeconds();

  // Call the actual update implementation
  update(std::move(surfaceTextures));

  // Record benchmark frame timing if tracker is active
  if (benchmarkTracker_) {
    const double endTime = getSeconds();
    const double renderTimeMs = (endTime - startTime) * 1000.0;
    recordBenchmarkFrame(renderTimeMs);

    // Check for periodic benchmark reporting
    checkBenchmarkPeriodicReport();

    // Check if benchmark has expired (only log once)
    if (benchmarkTracker_->hasBenchmarkExpired() && !benchmarkExpiredLogged_) {
      IGL_LOG_INFO("[IGL Benchmark] Benchmark duration expired, requesting exit\n");
      logFinalBenchmarkReport(true);
      appParamsRef().exitRequested = true;
      benchmarkExpiredLogged_ = true;
    }
  }

  // FPS throttling
  if (shellParams_ && shellParams_->fpsThrottleMs > 0) {
    const double endTime = getSeconds();
    const double frameTimeMs = (endTime - startTime) * 1000.0;
    const double targetMs =
        shellParams_->fpsThrottleRandom
            ? static_cast<double>(1 + (std::rand() % shellParams_->fpsThrottleMs))
            : static_cast<double>(shellParams_->fpsThrottleMs);
    if (frameTimeMs < targetMs) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(static_cast<int>(targetMs - frameTimeMs)));
    }
  }

  frameCount_++;
}

} // namespace igl::shell
