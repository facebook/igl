/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class EmptySession : public RenderSession {
 public:
  EmptySession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
};

} // namespace igl::shell
