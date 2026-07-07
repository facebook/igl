/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/shared/renderSession/BenchmarkTracker.h>

#include <limits>
#include <string>
#include <thread>

namespace igl::shell::tests {

// ---------------------------------------------------------------------------
// RenderTimeOverflowRecord defaults
// ---------------------------------------------------------------------------

TEST(RenderTimeOverflowRecordTest, DefaultValues) {
  const RenderTimeOverflowRecord record;
  EXPECT_EQ(record.minRenderTimeMs, std::numeric_limits<double>::max());
  EXPECT_EQ(record.maxRenderTimeMs, 0.0);
  EXPECT_EQ(record.sampleCount, 0u);
}

// ---------------------------------------------------------------------------
// RenderTimeStats defaults
// ---------------------------------------------------------------------------

TEST(RenderTimeStatsTest, DefaultValues) {
  const RenderTimeStats stats;
  EXPECT_EQ(stats.minRenderTimeMs, std::numeric_limits<double>::max());
  EXPECT_EQ(stats.maxRenderTimeMs, 0.0);
  EXPECT_EQ(stats.avgRenderTimeMs, 0.0);
  EXPECT_EQ(stats.minFps, 0.0);
  EXPECT_EQ(stats.maxFps, 0.0);
  EXPECT_EQ(stats.avgFps, 0.0);
  EXPECT_EQ(stats.totalSamples, 0u);
  EXPECT_FALSE(stats.hasHiccup);
  EXPECT_EQ(stats.hiccupThresholdMs, 0.0);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker construction
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, DefaultConstruction) {
  const BenchmarkTracker tracker;
  EXPECT_EQ(tracker.getTotalFrameCount(), 0u);
  EXPECT_EQ(tracker.getOverflowRecordCount(), 0u);
  EXPECT_EQ(tracker.getRunningAverageMs(), 0.0);
  EXPECT_FALSE(tracker.wasLastFrameHiccup());
  EXPECT_EQ(tracker.getBenchmarkDuration(), BenchmarkTracker::kDefaultBenchmarkDurationMs);
}

TEST(BenchmarkTrackerTest, CustomBufferSize) {
  BenchmarkTracker tracker(50);
  for (int i = 0; i < 50; ++i) {
    tracker.recordRenderTime(10.0);
  }
  EXPECT_EQ(tracker.getOverflowRecordCount(), 0u);
  tracker.recordRenderTime(10.0);
  EXPECT_EQ(tracker.getOverflowRecordCount(), 1u);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — recording samples
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, RecordSingleSample) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(16.6);
  EXPECT_EQ(tracker.getTotalFrameCount(), 1u);
  EXPECT_DOUBLE_EQ(tracker.getRunningAverageMs(), 16.6);
}

TEST(BenchmarkTrackerTest, RecordMultipleSamples) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(10.0);
  tracker.recordRenderTime(20.0);
  tracker.recordRenderTime(30.0);
  EXPECT_EQ(tracker.getTotalFrameCount(), 3u);
  EXPECT_DOUBLE_EQ(tracker.getRunningAverageMs(), 20.0);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — computeStats()
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, ComputeStatsEmpty) {
  const BenchmarkTracker tracker;
  const RenderTimeStats stats = tracker.computeStats();
  EXPECT_EQ(stats.totalSamples, 0u);
  EXPECT_EQ(stats.avgRenderTimeMs, 0.0);
}

TEST(BenchmarkTrackerTest, ComputeStatsMinMaxAvg) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(10.0);
  tracker.recordRenderTime(20.0);
  tracker.recordRenderTime(30.0);

  const RenderTimeStats stats = tracker.computeStats();
  EXPECT_EQ(stats.totalSamples, 3u);
  EXPECT_DOUBLE_EQ(stats.minRenderTimeMs, 10.0);
  EXPECT_DOUBLE_EQ(stats.maxRenderTimeMs, 30.0);
  EXPECT_DOUBLE_EQ(stats.avgRenderTimeMs, 20.0);
}

TEST(BenchmarkTrackerTest, ComputeStatsFpsConversion) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(10.0);
  tracker.recordRenderTime(20.0);

  const RenderTimeStats stats = tracker.computeStats();
  EXPECT_DOUBLE_EQ(stats.maxFps, 1000.0 / 10.0);
  EXPECT_DOUBLE_EQ(stats.minFps, 1000.0 / 20.0);
  EXPECT_DOUBLE_EQ(stats.avgFps, 1000.0 / 15.0);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — computeRecentStats()
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, ComputeRecentStatsEmpty) {
  const BenchmarkTracker tracker;
  const RenderTimeStats stats = tracker.computeRecentStats();
  EXPECT_EQ(stats.totalSamples, 0u);
}

TEST(BenchmarkTrackerTest, ComputeRecentStatsMatchesFullWhenNoOverflow) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(5.0);
  tracker.recordRenderTime(15.0);
  tracker.recordRenderTime(25.0);

  const RenderTimeStats full = tracker.computeStats();
  const RenderTimeStats recent = tracker.computeRecentStats();
  EXPECT_EQ(full.totalSamples, recent.totalSamples);
  EXPECT_DOUBLE_EQ(full.minRenderTimeMs, recent.minRenderTimeMs);
  EXPECT_DOUBLE_EQ(full.maxRenderTimeMs, recent.maxRenderTimeMs);
  EXPECT_DOUBLE_EQ(full.avgRenderTimeMs, recent.avgRenderTimeMs);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — buffer overflow
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, BufferOverflowCreatesRecord) {
  BenchmarkTracker tracker(10);
  for (int i = 0; i < 10; ++i) {
    tracker.recordRenderTime(static_cast<double>(i + 1));
  }
  EXPECT_EQ(tracker.getOverflowRecordCount(), 0u);
  EXPECT_EQ(tracker.getTotalFrameCount(), 10u);

  tracker.recordRenderTime(11.0);
  EXPECT_EQ(tracker.getOverflowRecordCount(), 1u);
  EXPECT_EQ(tracker.getTotalFrameCount(), 11u);
}

TEST(BenchmarkTrackerTest, OverflowPreservesMinMax) {
  BenchmarkTracker tracker(5);
  tracker.recordRenderTime(100.0);
  tracker.recordRenderTime(1.0);
  tracker.recordRenderTime(50.0);
  tracker.recordRenderTime(75.0);
  tracker.recordRenderTime(25.0);

  tracker.recordRenderTime(60.0);
  EXPECT_EQ(tracker.getOverflowRecordCount(), 1u);

  const RenderTimeStats stats = tracker.computeStats();
  EXPECT_DOUBLE_EQ(stats.minRenderTimeMs, 1.0);
  EXPECT_DOUBLE_EQ(stats.maxRenderTimeMs, 100.0);
}

TEST(BenchmarkTrackerTest, MultipleOverflows) {
  BenchmarkTracker tracker(3);
  for (int i = 0; i < 10; ++i) {
    tracker.recordRenderTime(static_cast<double>(i + 1));
  }
  EXPECT_EQ(tracker.getTotalFrameCount(), 10u);
  EXPECT_EQ(tracker.getOverflowRecordCount(), 3u);

  const RenderTimeStats stats = tracker.computeStats();
  EXPECT_DOUBLE_EQ(stats.minRenderTimeMs, 1.0);
  EXPECT_DOUBLE_EQ(stats.maxRenderTimeMs, 10.0);
}

TEST(BenchmarkTrackerTest, RecentStatsAfterOverflow) {
  BenchmarkTracker tracker(5);
  tracker.recordRenderTime(100.0);
  tracker.recordRenderTime(200.0);
  tracker.recordRenderTime(300.0);
  tracker.recordRenderTime(400.0);
  tracker.recordRenderTime(500.0);

  tracker.recordRenderTime(10.0);
  tracker.recordRenderTime(20.0);

  const RenderTimeStats recent = tracker.computeRecentStats();
  EXPECT_EQ(recent.totalSamples, 2u);
  EXPECT_DOUBLE_EQ(recent.minRenderTimeMs, 10.0);
  EXPECT_DOUBLE_EQ(recent.maxRenderTimeMs, 20.0);

  const RenderTimeStats full = tracker.computeStats();
  EXPECT_EQ(full.totalSamples, 7u);
  EXPECT_DOUBLE_EQ(full.minRenderTimeMs, 10.0);
  EXPECT_DOUBLE_EQ(full.maxRenderTimeMs, 500.0);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — hiccup detection
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, NoHiccupWithFewSamples) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(16.6);
  tracker.recordRenderTime(1000.0);
  EXPECT_FALSE(tracker.wasLastFrameHiccup());
}

TEST(BenchmarkTrackerTest, HiccupDetectedAfterEnoughSamples) {
  BenchmarkTracker tracker;
  for (int i = 0; i < 20; ++i) {
    tracker.recordRenderTime(16.6);
  }
  EXPECT_FALSE(tracker.wasLastFrameHiccup());

  tracker.recordRenderTime(200.0);
  EXPECT_TRUE(tracker.wasLastFrameHiccup());
}

TEST(BenchmarkTrackerTest, CustomHiccupMultiplier) {
  BenchmarkTracker tracker;
  tracker.setHiccupMultiplier(2.0);
  for (int i = 0; i < 20; ++i) {
    tracker.recordRenderTime(10.0);
  }

  tracker.recordRenderTime(25.0);
  EXPECT_TRUE(tracker.wasLastFrameHiccup());
}

TEST(BenchmarkTrackerTest, NormalFrameNotHiccup) {
  BenchmarkTracker tracker;
  for (int i = 0; i < 20; ++i) {
    tracker.recordRenderTime(16.6);
  }
  tracker.recordRenderTime(17.0);
  EXPECT_FALSE(tracker.wasLastFrameHiccup());
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — setters and getters
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, SetBenchmarkDuration) {
  BenchmarkTracker tracker;
  tracker.setBenchmarkDuration(5000);
  EXPECT_EQ(tracker.getBenchmarkDuration(), 5000u);
}

TEST(BenchmarkTrackerTest, ZeroDurationMeansNoLimit) {
  BenchmarkTracker tracker;
  tracker.setBenchmarkDuration(0);
  EXPECT_FALSE(tracker.hasBenchmarkExpired());
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — reset()
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, ResetClearsAllState) {
  BenchmarkTracker tracker(5);
  for (int i = 0; i < 10; ++i) {
    tracker.recordRenderTime(static_cast<double>(i + 1));
  }
  EXPECT_GT(tracker.getTotalFrameCount(), 0u);
  EXPECT_GT(tracker.getOverflowRecordCount(), 0u);

  tracker.reset();
  EXPECT_EQ(tracker.getTotalFrameCount(), 0u);
  EXPECT_EQ(tracker.getOverflowRecordCount(), 0u);
  EXPECT_EQ(tracker.getRunningAverageMs(), 0.0);
  EXPECT_FALSE(tracker.wasLastFrameHiccup());
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — getRunningAverageMs()
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, RunningAverageEmpty) {
  const BenchmarkTracker tracker;
  EXPECT_EQ(tracker.getRunningAverageMs(), 0.0);
}

TEST(BenchmarkTrackerTest, RunningAverageAccumulates) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(10.0);
  tracker.recordRenderTime(30.0);
  EXPECT_DOUBLE_EQ(tracker.getRunningAverageMs(), 20.0);

  tracker.recordRenderTime(20.0);
  EXPECT_DOUBLE_EQ(tracker.getRunningAverageMs(), 20.0);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — constants
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, DefaultConstants) {
  EXPECT_EQ(BenchmarkTracker::kDefaultBufferSize, 1000u);
  EXPECT_DOUBLE_EQ(BenchmarkTracker::kDefaultHiccupMultiplier, 3.0);
  EXPECT_EQ(BenchmarkTracker::kDefaultReportIntervalMs, 60000u);
  EXPECT_EQ(BenchmarkTracker::kDefaultBenchmarkDurationMs, 1800000u);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — setReportInterval() and periodic reporting
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, PeriodicReportRespectedInterval) {
  BenchmarkTracker tracker;
  tracker.setReportInterval(1);
  tracker.markPeriodicReportGenerated();
  EXPECT_FALSE(tracker.shouldGeneratePeriodicReport());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(tracker.shouldGeneratePeriodicReport());
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — getElapsedTimeMs()
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, ElapsedTimeIncreases) {
  BenchmarkTracker tracker;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  const double elapsed = tracker.getElapsedTimeMs();
  EXPECT_GE(elapsed, 50.0);
}

// ---------------------------------------------------------------------------
// BenchmarkTracker — hasBenchmarkExpired() positive case
// ---------------------------------------------------------------------------

TEST(BenchmarkTrackerTest, BenchmarkExpiresAfterDuration) {
  BenchmarkTracker tracker;
  tracker.setBenchmarkDuration(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(tracker.hasBenchmarkExpired());
}

// ---------------------------------------------------------------------------
// formatBenchmarkStats()
// ---------------------------------------------------------------------------

TEST(FormatBenchmarkStatsTest, ContainsKeyFields) {
  RenderTimeStats stats;
  stats.avgFps = 60.0;
  stats.minFps = 55.0;
  stats.maxFps = 65.0;
  stats.avgRenderTimeMs = 16.67;
  stats.minRenderTimeMs = 15.38;
  stats.maxRenderTimeMs = 18.18;
  stats.totalSamples = 100;
  stats.hasHiccup = false;

  const std::string result = formatBenchmarkStats(stats);
  EXPECT_NE(result.find("avg=60.0"), std::string::npos);
  EXPECT_NE(result.find("min=55.0"), std::string::npos);
  EXPECT_NE(result.find("max=65.0"), std::string::npos);
  EXPECT_NE(result.find("avg=16.67"), std::string::npos);
  EXPECT_NE(result.find("Samples: 100"), std::string::npos);
  EXPECT_EQ(result.find("HICCUP"), std::string::npos);
}

TEST(FormatBenchmarkStatsTest, HiccupMarker) {
  RenderTimeStats stats;
  stats.hasHiccup = true;
  stats.totalSamples = 1;
  stats.avgRenderTimeMs = 16.0;
  stats.minRenderTimeMs = 16.0;
  stats.maxRenderTimeMs = 16.0;
  stats.avgFps = 62.5;
  stats.minFps = 62.5;
  stats.maxFps = 62.5;

  const std::string result = formatBenchmarkStats(stats);
  EXPECT_NE(result.find("HICCUP"), std::string::npos);
}

TEST(FormatBenchmarkStatsTest, PrefixApplied) {
  RenderTimeStats stats;
  stats.totalSamples = 1;
  stats.avgRenderTimeMs = 10.0;
  stats.minRenderTimeMs = 10.0;
  stats.maxRenderTimeMs = 10.0;
  stats.avgFps = 100.0;
  stats.minFps = 100.0;
  stats.maxFps = 100.0;

  const std::string result = formatBenchmarkStats(stats, "[TEST] ");
  EXPECT_EQ(result.find("[TEST] "), 0u);
}

// ---------------------------------------------------------------------------
// generateFinalBenchmarkReport()
// ---------------------------------------------------------------------------

TEST(GenerateFinalBenchmarkReportTest, ContainsStatsFields) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(16.6);

  const std::string report = generateFinalBenchmarkReport(tracker, false);
  EXPECT_NE(report.find("BENCHMARK"), std::string::npos);
  EXPECT_NE(report.find("FPS Statistics"), std::string::npos);
  EXPECT_NE(report.find("Frame Time Statistics"), std::string::npos);
  EXPECT_NE(report.find("Overflow Records"), std::string::npos);
}

TEST(GenerateFinalBenchmarkReportTest, TimeoutStatus) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(16.6);

  const std::string report = generateFinalBenchmarkReport(tracker, true);
  EXPECT_NE(report.find("timeout"), std::string::npos);
}

TEST(GenerateFinalBenchmarkReportTest, NormalTerminationStatus) {
  BenchmarkTracker tracker;
  tracker.recordRenderTime(16.6);

  const std::string report = generateFinalBenchmarkReport(tracker, false);
  EXPECT_NE(report.find("terminated normally"), std::string::npos);
}

} // namespace igl::shell::tests
