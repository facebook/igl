/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <shell/shared/renderSession/RenderSessionConfig.h>
#include <shell/shared/renderSession/RenderSessionWindowConfig.h>
#include <shell/shared/renderSession/ShellType.h>
#include <vector>

namespace igl::shell {

class Platform;
class RenderSession;

class IRenderSessionFactory {
 public:
  virtual ~IRenderSessionFactory() noexcept = default;

  // Used on desktop platforms to configure the window hosting render sessions
  virtual RenderSessionWindowConfig requestedWindowConfig(
      ShellType /* shellType */,
      RenderSessionWindowConfig suggestedConfig) {
    return suggestedConfig;
  }

  // Used to configure individual render sessions
  virtual std::vector<RenderSessionConfig> requestedSessionConfigs(
      ShellType /* shellType */,
      std::vector<RenderSessionConfig> suggestedConfigs) {
    std::vector<RenderSessionConfig> requestedConfigs;
    requestedConfigs.reserve(suggestedConfigs.size());
    for (auto& suggestedConfig : suggestedConfigs) {
      if (suggestedConfig.backendVersion.flavor != igl::BackendFlavor::Invalid) {
        requestedConfigs.push_back(suggestedConfig);
      }
    }
    return requestedConfigs;
  }

  virtual std::unique_ptr<RenderSession> createRenderSession(
      std::shared_ptr<Platform> platform) noexcept = 0;
};

} // namespace igl::shell
