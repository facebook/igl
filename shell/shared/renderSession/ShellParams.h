/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <shell/shared/renderSession/Hands.h>
#include <shell/shared/renderSession/RenderMode.h>
#include <shell/shared/renderSession/ViewParams.h>
#include <igl/ColorSpace.h>
#include <igl/Common.h>
#include <igl/TextureFormat.h>

namespace igl::shell {

struct BenchmarkRenderSessionParams {
  size_t renderSessionTimeoutMs = 2000;
  size_t numSessionsToRun = 10;
  bool logReporter = false;
  bool offscreenRenderingOnly = false;
  std::vector<std::pair<std::string, std::string>> customParams;

  /// @brief Duration for the benchmark run in milliseconds (default: 30 minutes)
  /// When this time elapses, the benchmark will complete and generate a final report.
  /// Set to 0 for no time limit.
  size_t benchmarkDurationMs = 30 * 60 * 1000;

  /// @brief Interval between periodic FPS reports in milliseconds (default: 1 minute)
  size_t reportIntervalMs = 2 * 1000;

  /// @brief Multiplier for hiccup detection: frame time > avgFrameTime * multiplier = hiccup
  double hiccupMultiplier = 3.0;

  /// @brief Size of the circular buffer for storing render times
  size_t renderTimeBufferSize = 1000;
};

struct ShellParams {
  std::vector<ViewParams> viewParams;
  RenderMode renderMode = RenderMode::Mono;
  bool shellControlsViewParams = false;
  bool rightHandedCoordinateSystem = false;
  glm::vec2 viewportSize = glm::vec2(1024.0f, 768.0f);
  glm::ivec2 nativeSurfaceDimensions = glm::ivec2(2048, 1536);
  float viewportScale = 1.f;
  bool shouldPresent = true;
  std::optional<Color> clearColorValue = {};
  std::array<HandMesh, 2> handMeshes = {};
  std::array<HandTracking, 2> handTracking = {};
  std::string screenshotFileName = "screenshot.png";
  uint32_t screenshotNumber = ~0u; // frame number to save as a screenshot in headless more
  bool isHeadless = false;
  bool enableVulkanValidationLayers = true;
  std::optional<BenchmarkRenderSessionParams> benchmarkParams = {};

  // FPS throttling for testing race conditions
  uint32_t fpsThrottleMs = 0; // 0 = disabled, >0 = delay in milliseconds per frame
  uint32_t freezeAtFrame = ~0u; // frame number to freeze at (~0u = disabled)
  bool fpsThrottleRandom = false; // if true, throttle is random in range [1, fpsThrottleMs]
};

std::vector<std::string> convertArgvToParams(int argc, char** argv);

void parseShellParams(const std::vector<std::string>& args, ShellParams& shellParams);
} // namespace igl::shell
