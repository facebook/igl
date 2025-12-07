/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <limits>
#include <string>
#include <vector>

namespace igl::shell {

/// @brief Stores min/max render time data when circular buffer overflows
struct RenderTimeOverflowRecord {
  double minRenderTimeMs = std::numeric_limits<double>::max();
  double maxRenderTimeMs = 0.0;
  size_t sampleCount = 0;
};

/// @brief Statistics computed from render time samples
struct RenderTimeStats {
  double minRenderTimeMs = std::numeric_limits<double>::max();
  double maxRenderTimeMs = 0.0;
  double avgRenderTimeMs = 0.0;
  double minFps = 0.0;
  double maxFps = 0.0;
  double avgFps = 0.0;
  size_t totalSamples = 0;
  bool hasHiccup = false;
  double hiccupThresholdMs = 0.0;
};

/// @brief Tracks render times and provides benchmark statistics for IGL shell
///
/// This class maintains a circular buffer of render times and computes
/// performance statistics including FPS metrics. When the buffer overflows,
/// min/max values are preserved in overflow records to maintain historical data.
class BenchmarkTracker {
 public:
  static constexpr size_t kDefaultBufferSize = 1000;
  static constexpr double kDefaultHiccupMultiplier =
      3.0; // Frame is a hiccup if > 3x average frame time
  static constexpr size_t kDefaultReportIntervalMs = 60000; // 1 minute
  static constexpr size_t kDefaultBenchmarkDurationMs = 30 * 60 * 1000; // 30 minutes

  explicit BenchmarkTracker(size_t bufferSize = kDefaultBufferSize);

  /// @brief Records a render time sample
  /// @param renderTimeMs The time in milliseconds for the render call
  void recordRenderTime(double renderTimeMs);

  /// @brief Checks if it's time to generate a periodic report
  /// @return true if a report should be generated
  [[nodiscard]] bool shouldGeneratePeriodicReport() const;

  /// @brief Marks that a periodic report was generated
  void markPeriodicReportGenerated();

  /// @brief Computes current statistics from all available data
  /// @return RenderTimeStats containing min/max/average values
  [[nodiscard]] RenderTimeStats computeStats() const;

  /// @brief Computes statistics from just the current buffer (recent samples)
  /// @return RenderTimeStats for recent samples only
  [[nodiscard]] RenderTimeStats computeRecentStats() const;

  /// @brief Checks if the benchmark duration has been exceeded
  /// @return true if the benchmark should stop
  [[nodiscard]] bool hasBenchmarkExpired() const;

  /// @brief Gets elapsed time since benchmark started
  /// @return Elapsed time in milliseconds
  [[nodiscard]] double getElapsedTimeMs() const;

  /// @brief Sets the benchmark duration
  /// @param durationMs Duration in milliseconds (0 = no limit)
  void setBenchmarkDuration(size_t durationMs);

  /// @brief Gets the benchmark duration setting
  /// @return Duration in milliseconds
  [[nodiscard]] size_t getBenchmarkDuration() const;

  /// @brief Sets the report interval
  /// @param intervalMs Interval in milliseconds between periodic reports
  void setReportInterval(size_t intervalMs);

  /// @brief Sets the hiccup detection multiplier
  /// @param multiplier A frame is considered a hiccup if > multiplier * average frame time
  void setHiccupMultiplier(double multiplier);

  /// @brief Resets all tracking data and restarts the benchmark timer
  void reset();

  /// @brief Gets the total number of frames tracked (including overflowed)
  [[nodiscard]] size_t getTotalFrameCount() const;

  /// @brief Gets the number of overflow records
  [[nodiscard]] size_t getOverflowRecordCount() const;

  /// @brief Detects if the most recent frame was a significant hiccup
  /// @return true if the last recorded frame was a hiccup
  [[nodiscard]] bool wasLastFrameHiccup() const;

  /// @brief Gets the current running average (for hiccup detection)
  [[nodiscard]] double getRunningAverageMs() const;

 private:
  void flushBufferToOverflow();
  [[nodiscard]] RenderTimeStats computeStatsFromBuffer(const std::vector<double>& buffer) const;

  std::vector<double> circularBuffer_;
  size_t bufferIndex_ = 0;
  size_t bufferCount_ = 0; // Actual number of samples in buffer
  size_t bufferCapacity_;

  std::vector<RenderTimeOverflowRecord> overflowRecords_;

  double runningSum_ = 0.0;
  size_t totalSampleCount_ = 0;

  double hiccupMultiplier_ = kDefaultHiccupMultiplier;
  bool lastFrameWasHiccup_ = false;
  double lastRenderTimeMs_ = 0.0;

  std::chrono::steady_clock::time_point startTime_;
  std::chrono::steady_clock::time_point lastReportTime_;
  size_t reportIntervalMs_ = kDefaultReportIntervalMs;
  size_t benchmarkDurationMs_ = kDefaultBenchmarkDurationMs;
};

/// @brief Generates a formatted log string for benchmark statistics
/// @param stats The statistics to format
/// @param prefix Optional prefix for the log line
/// @return Formatted string suitable for IGL_LOG_INFO
std::string formatBenchmarkStats(const RenderTimeStats& stats, const char* prefix = "");

/// @brief Generates the final benchmark report
/// @param tracker The benchmark tracker with all data
/// @param wasTimeout true if the benchmark ended due to timeout
/// @return Formatted multi-line string for the final report
std::string generateFinalBenchmarkReport(const BenchmarkTracker& tracker, bool wasTimeout);

} // namespace igl::shell
