/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell::tests {

// ---------------------------------------------------------------------------
// convertArgvToParams()
// ---------------------------------------------------------------------------

TEST(ConvertArgvToParamsTest, EmptyArgv) {
  const std::vector<std::string> params = convertArgvToParams(0, nullptr);
  EXPECT_TRUE(params.empty());
}

TEST(ConvertArgvToParamsTest, PreservesOrderAndValues) {
  char arg0[] = "prog";
  char arg1[] = "--headless";
  char arg2[] = "--sessions";
  char* argv[] = {arg0, arg1, arg2};

  const std::vector<std::string> params = convertArgvToParams(3, argv);
  ASSERT_EQ(params.size(), 3u);
  EXPECT_EQ(params[0], "prog");
  EXPECT_EQ(params[1], "--headless");
  EXPECT_EQ(params[2], "--sessions");
}

// ---------------------------------------------------------------------------
// parseShellParams() — defaults untouched by an empty/unknown arg list
// ---------------------------------------------------------------------------

TEST(ParseShellParamsTest, EmptyArgsLeavesDefaults) {
  ShellParams shellParams;
  parseShellParams({}, shellParams);
  EXPECT_FALSE(shellParams.isHeadless);
  EXPECT_TRUE(shellParams.shouldPresent);
  EXPECT_TRUE(shellParams.enableVulkanValidationLayers);
  EXPECT_EQ(shellParams.screenshotFileName, "screenshot.png");
  EXPECT_EQ(shellParams.screenshotNumber, ~0u);
  EXPECT_FALSE(shellParams.benchmarkParams.has_value());
}

TEST(ParseShellParamsTest, UnrecognizedFlagDoesNotEngageBenchmarkParams) {
  ShellParams shellParams;
  parseShellParams({"--totally-unknown-flag", "value"}, shellParams);
  // A bare "--" flag only registers as a custom benchmark param when benchmark
  // mode is otherwise engaged; with no recognized benchmark flag, "found"
  // stays false and the whole optional collapses back to nullopt.
  EXPECT_FALSE(shellParams.benchmarkParams.has_value());
}

// ---------------------------------------------------------------------------
// --headless
// ---------------------------------------------------------------------------

TEST(ParseShellParamsTest, HeadlessSetsExpectedDefaults) {
  ShellParams shellParams;
  parseShellParams({"--headless"}, shellParams);
  EXPECT_TRUE(shellParams.isHeadless);
  EXPECT_FALSE(shellParams.shouldPresent);
  EXPECT_EQ(shellParams.screenshotNumber, 0u);
}

TEST(ParseShellParamsTest, HeadlessDoesNotOverrideExplicitScreenshotNumber) {
  ShellParams shellParams;
  parseShellParams({"--screenshot-number", "5", "--headless"}, shellParams);
  EXPECT_TRUE(shellParams.isHeadless);
  EXPECT_EQ(shellParams.screenshotNumber, 5u);
}

TEST(ParseShellParamsTest, ScreenshotNumberAfterHeadlessOverridesZero) {
  ShellParams shellParams;
  parseShellParams({"--headless", "--screenshot-number", "7"}, shellParams);
  EXPECT_EQ(shellParams.screenshotNumber, 7u);
}

// ---------------------------------------------------------------------------
// Simple flags
// ---------------------------------------------------------------------------

TEST(ParseShellParamsTest, DisableVulkanValidationLayers) {
  ShellParams shellParams;
  parseShellParams({"--disable-vulkan-validation-layers"}, shellParams);
  EXPECT_FALSE(shellParams.enableVulkanValidationLayers);
}

TEST(ParseShellParamsTest, ScreenshotFile) {
  ShellParams shellParams;
  parseShellParams({"--screenshot-file", "out.png"}, shellParams);
  EXPECT_EQ(shellParams.screenshotFileName, "out.png");
}

TEST(ParseShellParamsTest, ScreenshotFileMissingValueLeavesDefault) {
  ShellParams shellParams;
  parseShellParams({"--screenshot-file"}, shellParams);
  EXPECT_EQ(shellParams.screenshotFileName, "screenshot.png");
}

TEST(ParseShellParamsTest, FpsThrottle) {
  ShellParams shellParams;
  parseShellParams({"--fps-throttle", "16"}, shellParams);
  EXPECT_EQ(shellParams.fpsThrottleMs, 16u);
}

TEST(ParseShellParamsTest, FpsThrottleRandom) {
  ShellParams shellParams;
  parseShellParams({"--fps-throttle-random"}, shellParams);
  EXPECT_TRUE(shellParams.fpsThrottleRandom);
}

TEST(ParseShellParamsTest, FreezeAtFrame) {
  ShellParams shellParams;
  parseShellParams({"--freeze-at-frame", "42"}, shellParams);
  EXPECT_EQ(shellParams.freezeAtFrame, 42u);
}

TEST(ParseShellParamsTest, UseTimerRendering) {
  ShellParams shellParams;
  parseShellParams({"--use-timer-rendering"}, shellParams);
  EXPECT_TRUE(shellParams.useTimerRendering);
}

// ---------------------------------------------------------------------------
// --viewport-size
// ---------------------------------------------------------------------------

TEST(ParseShellParamsTest, ViewportSizeValid) {
  ShellParams shellParams;
  parseShellParams({"--viewport-size", "1920x1080"}, shellParams);
  EXPECT_FLOAT_EQ(shellParams.viewportSize.x, 1920.0f);
  EXPECT_FLOAT_EQ(shellParams.viewportSize.y, 1080.0f);
}

TEST(ParseShellParamsTest, ViewportSizeInvalidFormatLeavesDefault) {
  ShellParams shellParams;
  parseShellParams({"--viewport-size", "not-a-size"}, shellParams);
  EXPECT_FLOAT_EQ(shellParams.viewportSize.x, 1024.0f);
  EXPECT_FLOAT_EQ(shellParams.viewportSize.y, 768.0f);
}

TEST(ParseShellParamsTest, ViewportSizeZeroDimensionLeavesDefault) {
  ShellParams shellParams;
  parseShellParams({"--viewport-size", "0x0"}, shellParams);
  EXPECT_FLOAT_EQ(shellParams.viewportSize.x, 1024.0f);
  EXPECT_FLOAT_EQ(shellParams.viewportSize.y, 768.0f);
}

// ---------------------------------------------------------------------------
// --force-multiview
// ---------------------------------------------------------------------------

TEST(ParseShellParamsTest, ForceMultiview) {
  ShellParams shellParams;
  parseShellParams({"--force-multiview"}, shellParams);
  EXPECT_TRUE(shellParams.forceMultiview);
  EXPECT_EQ(shellParams.renderMode, RenderMode::SinglePassStereo);
  ASSERT_EQ(shellParams.viewParams.size(), 2u);
  EXPECT_EQ(shellParams.viewParams[0].viewIndex, 0u);
  EXPECT_EQ(shellParams.viewParams[1].viewIndex, 1u);
}

TEST(ParseShellParamsTest, ForceMultiviewInsideBenchmarkDoesNotBecomeCustomParam) {
  ShellParams shellParams;
  parseShellParams({"--benchmark", "--force-multiview"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_TRUE(shellParams.benchmarkParams->customParams.empty());
  EXPECT_TRUE(shellParams.forceMultiview);
}

// ---------------------------------------------------------------------------
// Benchmark params — engagement and defaults
// ---------------------------------------------------------------------------

TEST(ParseShellParamsTest, BenchmarkFlagAloneEngagesDefaults) {
  ShellParams shellParams;
  parseShellParams({"--benchmark"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_EQ(shellParams.benchmarkParams->renderSessionTimeoutMs, 2000u);
  EXPECT_EQ(shellParams.benchmarkParams->numSessionsToRun, 10u);
  EXPECT_FALSE(shellParams.benchmarkParams->logReporter);
  EXPECT_FALSE(shellParams.benchmarkParams->offscreenRenderingOnly);
}

TEST(ParseShellParamsTest, BenchmarkShortFlagAlsoEngages) {
  ShellParams shellParams;
  parseShellParams({"-b"}, shellParams);
  EXPECT_TRUE(shellParams.benchmarkParams.has_value());
}

TEST(ParseShellParamsTest, BenchmarkTimeoutAndSessions) {
  ShellParams shellParams;
  parseShellParams({"--timeout", "5000", "--sessions", "3"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_EQ(shellParams.benchmarkParams->renderSessionTimeoutMs, 5000u);
  EXPECT_EQ(shellParams.benchmarkParams->numSessionsToRun, 3u);
}

TEST(ParseShellParamsTest, BenchmarkShortFlagsForTimeoutAndSessions) {
  ShellParams shellParams;
  parseShellParams({"-t", "1234", "-s", "9"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_EQ(shellParams.benchmarkParams->renderSessionTimeoutMs, 1234u);
  EXPECT_EQ(shellParams.benchmarkParams->numSessionsToRun, 9u);
}

TEST(ParseShellParamsTest, BenchmarkLogReporter) {
  ShellParams shellParams;
  parseShellParams({"--log-reporter"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_TRUE(shellParams.benchmarkParams->logReporter);
}

TEST(ParseShellParamsTest, BenchmarkOffscreenOnlyForcesShouldPresentFalse) {
  ShellParams shellParams;
  ASSERT_TRUE(shellParams.shouldPresent);
  parseShellParams({"--offscreen-only"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_TRUE(shellParams.benchmarkParams->offscreenRenderingOnly);
  EXPECT_FALSE(shellParams.shouldPresent);
}

TEST(ParseShellParamsTest, BenchmarkDurationLongAndAliasFlag) {
  ShellParams shellParams;
  parseShellParams({"--benchmark-duration", "60000"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_EQ(shellParams.benchmarkParams->benchmarkDurationMs, 60000u);

  ShellParams shellParams2;
  parseShellParams({"--run-time", "90000"}, shellParams2);
  ASSERT_TRUE(shellParams2.benchmarkParams.has_value());
  EXPECT_EQ(shellParams2.benchmarkParams->benchmarkDurationMs, 90000u);
}

TEST(ParseShellParamsTest, BenchmarkReportInterval) {
  ShellParams shellParams;
  parseShellParams({"--report-interval", "500"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_EQ(shellParams.benchmarkParams->reportIntervalMs, 500u);
}

TEST(ParseShellParamsTest, BenchmarkHiccupMultiplier) {
  ShellParams shellParams;
  parseShellParams({"--hiccup-multiplier", "2.5"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_DOUBLE_EQ(shellParams.benchmarkParams->hiccupMultiplier, 2.5);
}

TEST(ParseShellParamsTest, BenchmarkRenderBufferSize) {
  ShellParams shellParams;
  parseShellParams({"--render-buffer-size", "250"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  EXPECT_EQ(shellParams.benchmarkParams->renderTimeBufferSize, 250u);
}

TEST(ParseShellParamsTest, BenchmarkCustomParamsCollected) {
  ShellParams shellParams;
  parseShellParams({"--benchmark", "--myFlag", "myValue"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  ASSERT_EQ(shellParams.benchmarkParams->customParams.size(), 1u);
  EXPECT_EQ(shellParams.benchmarkParams->customParams[0].first, "myFlag");
  EXPECT_EQ(shellParams.benchmarkParams->customParams[0].second, "myValue");
}

TEST(ParseShellParamsTest, BenchmarkCustomParamWithoutValue) {
  ShellParams shellParams;
  parseShellParams({"--benchmark", "--flagOnly"}, shellParams);
  ASSERT_TRUE(shellParams.benchmarkParams.has_value());
  ASSERT_EQ(shellParams.benchmarkParams->customParams.size(), 1u);
  EXPECT_EQ(shellParams.benchmarkParams->customParams[0].first, "flagOnly");
  EXPECT_TRUE(shellParams.benchmarkParams->customParams[0].second.empty());
}

} // namespace igl::shell::tests
