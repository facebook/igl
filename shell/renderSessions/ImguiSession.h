/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/imgui/Session.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class ImguiSession : public RenderSession {
 public:
  explicit ImguiSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<igl::ICommandQueue> _commandQueue;
  std::shared_ptr<igl::IFramebuffer> _outputFramebuffer;
  std::unique_ptr<iglu::imgui::Session> _imguiSession;
};

} // namespace igl::shell
