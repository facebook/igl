/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/ShellParams.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace igl::shell {
namespace {
std::optional<BenchmarkRenderSessionParams> parseBenchmarkRenderSessionParams(
    const std::vector<std::string>& args) {
  // Flag to track if any benchmark-related parameters were found
  bool benchmarkParamsFound = false;

  // Create a params struct with default values
  BenchmarkRenderSessionParams benchmarkParams;

  // Parse command line arguments
  for (size_t i = 0; i < args.size(); i++) {
    const std::string& arg = args[i];

    // Check for timeout parameter
    if (arg == "--timeout" || arg == "-t") {
      if (i + 1 < args.size()) {
        benchmarkParams.renderSessionTimeoutMs = std::stoul(args[++i]);
        benchmarkParamsFound = true;
      }
    }
    // Check for number of sessions parameter
    else if (arg == "--sessions" || arg == "-s") {
      if (i + 1 < args.size()) {
        benchmarkParams.numSessionsToRun = std::stoul(args[++i]);
        benchmarkParamsFound = true;
      }
    }
    // Check for log reporter flag
    else if (arg == "--log-reporter" || arg == "-l") {
      benchmarkParams.logReporter = true;
      benchmarkParamsFound = true;
    }
    // Check for offscreen rendering flag
    else if (arg == "--offscreen-only" || arg == "-o") {
      benchmarkParams.offscreenRenderingOnly = true;
      benchmarkParamsFound = true;
    }
    // Check for benchmark mode flag (enables benchmark mode without specific params)
    else if (arg == "--benchmark" || arg == "-b") {
      benchmarkParamsFound = true;
    }
  }

  // Return the params if any benchmark-related parameters were found, otherwise return nullopt
  return benchmarkParamsFound ? std::optional<BenchmarkRenderSessionParams>(benchmarkParams)
                              : std::nullopt;
}
} // namespace

std::vector<std::string> convertArgvToParams(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; i++) {
    args.emplace_back(argv[i]);
  }
  return args;
}

void parseShellParams(const std::vector<std::string>& args, ShellParams& shellParams) {
  // Parse benchmark parameters using existing function
  shellParams.benchmarkParams = parseBenchmarkRenderSessionParams(args);

  // Parse other shell parameters
  for (size_t i = 0; i < args.size(); i++) {
    const std::string& arg = args[i];

    if (arg == "--headless") {
      shellParams.isHeadless = true;
      shellParams.screenshotNumber =
          shellParams.screenshotNumber != ~0 ? shellParams.screenshotNumber : 0;
    } else if (arg == "--disable-vulkan-validation-layers") {
      shellParams.enableVulkanValidationLayers = false;
    } else if (arg == "--screenshot-file") {
      if (i + 1 < args.size()) {
        shellParams.screenshotFileName = args[++i];
      }
    } else if (arg == "--screenshot-number") {
      if (i + 1 < args.size()) {
        shellParams.screenshotNumber = static_cast<uint32_t>(std::stoi(args[++i]));
      }
    } else if (arg == "--viewport-size") {
      if (i + 1 < args.size()) {
        const std::string& value = args[++i];
        unsigned int w = 0;
        unsigned int h = 0;
        if (sscanf(value.c_str(), "%ux%u", &w, &h) == 2) {
          if (w && h) {
            shellParams.viewportSize = glm::vec2(w, h);
          }
        }
      }
    }
  }
}

} // namespace igl::shell
