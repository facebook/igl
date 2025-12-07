/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/BenchmarkTracker.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace igl::shell {

BenchmarkTracker::BenchmarkTracker(size_t bufferSize) : bufferCapacity_(bufferSize) {
  circularBuffer_.resize(bufferCapacity_, 0.0);
  reset();
}

void BenchmarkTracker::recordRenderTime(double renderTimeMs) {
  // Detect hiccup before updating running average
  const double runningAvg = getRunningAverageMs();
  if (totalSampleCount_ > 10 && runningAvg > 0) {
    lastFrameWasHiccup_ = (renderTimeMs > runningAvg * hiccupMultiplier_);
  } else {
    lastFrameWasHiccup_ = false;
  }
  lastRenderTimeMs_ = renderTimeMs;

  // Check if buffer is full and needs to overflow
  if (bufferCount_ >= bufferCapacity_) {
    flushBufferToOverflow();
  }

  // Add sample to circular buffer
  circularBuffer_[bufferIndex_] = renderTimeMs;
  bufferIndex_ = (bufferIndex_ + 1) % bufferCapacity_;
  if (bufferCount_ < bufferCapacity_) {
    bufferCount_++;
  }

  // Update running statistics
  runningSum_ += renderTimeMs;
  totalSampleCount_++;
}

void BenchmarkTracker::flushBufferToOverflow() {
  if (bufferCount_ == 0) {
    return;
  }

  RenderTimeOverflowRecord record;
  record.sampleCount = bufferCount_;

  for (size_t i = 0; i < bufferCount_; ++i) {
    const double val = circularBuffer_[i];
    record.minRenderTimeMs = std::min(record.minRenderTimeMs, val);
    record.maxRenderTimeMs = std::max(record.maxRenderTimeMs, val);
  }

  overflowRecords_.push_back(record);

  // Reset buffer state
  bufferIndex_ = 0;
  bufferCount_ = 0;
}

bool BenchmarkTracker::shouldGeneratePeriodicReport() const {
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReportTime_);
  return static_cast<size_t>(elapsed.count()) >= reportIntervalMs_;
}

void BenchmarkTracker::markPeriodicReportGenerated() {
  lastReportTime_ = std::chrono::steady_clock::now();
}

RenderTimeStats BenchmarkTracker::computeStats() const {
  RenderTimeStats stats;
  stats.totalSamples = totalSampleCount_;

  if (totalSampleCount_ == 0) {
    return stats;
  }

  // Compute from current buffer
  for (size_t i = 0; i < bufferCount_; ++i) {
    const double val = circularBuffer_[i];
    stats.minRenderTimeMs = std::min(stats.minRenderTimeMs, val);
    stats.maxRenderTimeMs = std::max(stats.maxRenderTimeMs, val);
  }

  // Include overflow records
  for (const auto& record : overflowRecords_) {
    stats.minRenderTimeMs = std::min(stats.minRenderTimeMs, record.minRenderTimeMs);
    stats.maxRenderTimeMs = std::max(stats.maxRenderTimeMs, record.maxRenderTimeMs);
  }

  // Compute average
  stats.avgRenderTimeMs = runningSum_ / static_cast<double>(totalSampleCount_);

  // Convert to FPS (avoiding division by zero)
  if (stats.minRenderTimeMs > 0) {
    stats.maxFps = 1000.0 / stats.minRenderTimeMs;
  }
  if (stats.maxRenderTimeMs > 0) {
    stats.minFps = 1000.0 / stats.maxRenderTimeMs;
  }
  if (stats.avgRenderTimeMs > 0) {
    stats.avgFps = 1000.0 / stats.avgRenderTimeMs;
  }

  // Determine hiccup threshold
  stats.hiccupThresholdMs = stats.avgRenderTimeMs * hiccupMultiplier_;
  stats.hasHiccup = lastFrameWasHiccup_;

  return stats;
}

RenderTimeStats BenchmarkTracker::computeRecentStats() const {
  RenderTimeStats stats;
  stats.totalSamples = bufferCount_;

  if (bufferCount_ == 0) {
    return stats;
  }

  double sum = 0.0;
  for (size_t i = 0; i < bufferCount_; ++i) {
    const double val = circularBuffer_[i];
    stats.minRenderTimeMs = std::min(stats.minRenderTimeMs, val);
    stats.maxRenderTimeMs = std::max(stats.maxRenderTimeMs, val);
    sum += val;
  }

  stats.avgRenderTimeMs = sum / static_cast<double>(bufferCount_);

  // Convert to FPS
  if (stats.minRenderTimeMs > 0) {
    stats.maxFps = 1000.0 / stats.minRenderTimeMs;
  }
  if (stats.maxRenderTimeMs > 0) {
    stats.minFps = 1000.0 / stats.maxRenderTimeMs;
  }
  if (stats.avgRenderTimeMs > 0) {
    stats.avgFps = 1000.0 / stats.avgRenderTimeMs;
  }

  stats.hiccupThresholdMs = stats.avgRenderTimeMs * hiccupMultiplier_;
  stats.hasHiccup = lastFrameWasHiccup_;

  return stats;
}

bool BenchmarkTracker::hasBenchmarkExpired() const {
  if (benchmarkDurationMs_ == 0) {
    return false; // No limit
  }
  return getElapsedTimeMs() >= static_cast<double>(benchmarkDurationMs_);
}

double BenchmarkTracker::getElapsedTimeMs() const {
  const auto now = std::chrono::steady_clock::now();
  return static_cast<double>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count());
}

void BenchmarkTracker::setBenchmarkDuration(size_t durationMs) {
  benchmarkDurationMs_ = durationMs;
}

size_t BenchmarkTracker::getBenchmarkDuration() const {
  return benchmarkDurationMs_;
}

void BenchmarkTracker::setReportInterval(size_t intervalMs) {
  reportIntervalMs_ = intervalMs;
}

void BenchmarkTracker::setHiccupMultiplier(double multiplier) {
  hiccupMultiplier_ = multiplier;
}

void BenchmarkTracker::reset() {
  bufferIndex_ = 0;
  bufferCount_ = 0;
  overflowRecords_.clear();
  runningSum_ = 0.0;
  totalSampleCount_ = 0;
  lastFrameWasHiccup_ = false;
  lastRenderTimeMs_ = 0.0;
  startTime_ = std::chrono::steady_clock::now();
  lastReportTime_ = startTime_;
}

size_t BenchmarkTracker::getTotalFrameCount() const {
  return totalSampleCount_;
}

size_t BenchmarkTracker::getOverflowRecordCount() const {
  return overflowRecords_.size();
}

bool BenchmarkTracker::wasLastFrameHiccup() const {
  return lastFrameWasHiccup_;
}

double BenchmarkTracker::getRunningAverageMs() const {
  if (totalSampleCount_ == 0) {
    return 0.0;
  }
  return runningSum_ / static_cast<double>(totalSampleCount_);
}

std::string formatBenchmarkStats(const RenderTimeStats& stats, const char* prefix) {
  char buffer[512];
  snprintf(buffer,
           sizeof(buffer),
           "%sFPS: avg=%.1f, min=%.1f, max=%.1f | "
           "Frame time (ms): avg=%.2f, min=%.2f, max=%.2f | "
           "Samples: %zu%s",
           prefix,
           stats.avgFps,
           stats.minFps,
           stats.maxFps,
           stats.avgRenderTimeMs,
           stats.minRenderTimeMs,
           stats.maxRenderTimeMs,
           stats.totalSamples,
           stats.hasHiccup ? " [HICCUP DETECTED]" : "");
  return std::string(buffer);
}

std::string generateFinalBenchmarkReport(const BenchmarkTracker& tracker, bool wasTimeout) {
  const auto stats = tracker.computeStats();
  const double elapsedSec = tracker.getElapsedTimeMs() / 1000.0;
  const double elapsedMin = elapsedSec / 60.0;

  std::ostringstream oss;
  oss << "\n";
  oss << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
  oss << "║                        IGL BENCHMARK FINAL REPORT                            ║\n";
  oss << "╠══════════════════════════════════════════════════════════════════════════════╣\n";

  if (wasTimeout) {
    oss << "║  Status: COMPLETED SUCCESSFULLY (benchmark timeout reached)                  ║\n";
  } else {
    oss << "║  Status: COMPLETED (application terminated normally)                         ║\n";
  }

  char line[128];
  snprintf(line,
           sizeof(line),
           "║  Duration: %.1f minutes (%.1f seconds)                                    ",
           elapsedMin,
           elapsedSec);
  oss << line << "║\n";

  snprintf(line, sizeof(line), "║  Total Frames: %zu", stats.totalSamples);
  oss << line;
  // Pad to fit the box
  size_t len = strlen(line);
  for (size_t i = len; i < 80; ++i) {
    oss << " ";
  }
  oss << "║\n";

  oss << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
  oss << "║  FPS Statistics:                                                             ║\n";

  snprintf(line,
           sizeof(line),
           "║    Average: %.1f FPS                                                    ",
           stats.avgFps);
  oss << line << "║\n";

  snprintf(line,
           sizeof(line),
           "║    Minimum: %.1f FPS                                                    ",
           stats.minFps);
  oss << line << "║\n";

  snprintf(line,
           sizeof(line),
           "║    Maximum: %.1f FPS                                                    ",
           stats.maxFps);
  oss << line << "║\n";

  oss << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
  oss << "║  Frame Time Statistics:                                                      ║\n";

  snprintf(line,
           sizeof(line),
           "║    Average: %.2f ms                                                     ",
           stats.avgRenderTimeMs);
  oss << line << "║\n";

  snprintf(line,
           sizeof(line),
           "║    Minimum: %.2f ms                                                     ",
           stats.minRenderTimeMs);
  oss << line << "║\n";

  snprintf(line,
           sizeof(line),
           "║    Maximum: %.2f ms                                                     ",
           stats.maxRenderTimeMs);
  oss << line << "║\n";

  oss << "╠══════════════════════════════════════════════════════════════════════════════╣\n";

  snprintf(line,
           sizeof(line),
           "║  Overflow Records: %zu (each contains min/max from %zu samples)        ",
           tracker.getOverflowRecordCount(),
           BenchmarkTracker::kDefaultBufferSize);
  oss << line << "║\n";

  oss << "╚══════════════════════════════════════════════════════════════════════════════╝\n";

  return oss.str();
}

} // namespace igl::shell
