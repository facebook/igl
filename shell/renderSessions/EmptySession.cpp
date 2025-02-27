/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "EmptySession.h"

namespace igl::shell {

void EmptySession::initialize() noexcept {
  getPlatform().getDevice();
}

void EmptySession::update(SurfaceTextures surfaceTextures) noexcept {}

} // namespace igl::shell
