/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/RenderSession.h>

#include <chrono>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

RenderSession::RenderSession(std::shared_ptr<Platform> platform) :
  platform_(std::move(platform)), appParams_(std::make_shared<AppParams>()) {}

void RenderSession::updateDisplayScale(float scale) noexcept {
  platform_->getDisplayContext().scale = scale;
}

float RenderSession::pixelsPerPoint() const noexcept {
  return platform_->getDisplayContext().pixelsPerPoint;
}

void RenderSession::setPixelsPerPoint(float scale) noexcept {
  platform_->getDisplayContext().pixelsPerPoint = scale;
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

float RenderSession::getDeltaSeconds() noexcept {
  const double newTime = getSeconds();
  const float deltaSeconds = float(newTime - lastTime_);
  lastTime_ = newTime;
  return deltaSeconds;
}

double RenderSession::getSeconds() noexcept {
  return std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

void RenderSession::setPreferredClearColor(const igl::Color& color) noexcept {
  preferredClearColor_ = color;
}

Color RenderSession::getPreferredClearColor() noexcept {
  return preferredClearColor_.has_value() ? preferredClearColor_.value()
                                          : platform()->getDevice().backendDebugColor();
}

} // namespace igl::shell
