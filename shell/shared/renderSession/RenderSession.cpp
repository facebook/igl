/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/RenderSession.h>

#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {
RenderSession::RenderSession(std::shared_ptr<Platform> platform) :
  platform_(std::move(platform)), appParams_(std::make_unique<AppParams>()) {}

RenderSession::~RenderSession() noexcept = default;

void RenderSession::update(igl::SurfaceTextures surfaceTextures) noexcept {}

void RenderSession::updateDisplayScale(float scale) noexcept {
  platform_->getDisplayContext().scale = scale;
}

void RenderSession::setShellParams(const ShellParams& shellParams) noexcept {
  shellParams_ = &shellParams;
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

} // namespace igl::shell
