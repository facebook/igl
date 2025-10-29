/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <optional>
#include <shell/shared/renderSession/Hands.h>
#include <shell/shared/renderSession/RenderMode.h>
#include <shell/shared/renderSession/ViewParams.h>
#include <string>
#include <vector>
#include <igl/ColorSpace.h>
#include <igl/Common.h>
#include <igl/TextureFormat.h>

namespace igl::shell {

struct BenchmarkRenderSessionParams {
  size_t renderSessionTimeoutMs = 2000;
  size_t numSessionsToRun = 10;
  bool logReporter = false;
  bool offscreenRenderingOnly = false;
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
};

std::vector<std::string> convertArgvToParams(int argc, char** argv);

void parseShellParams(const std::vector<std::string>& args, ShellParams& shellParams);
} // namespace igl::shell
