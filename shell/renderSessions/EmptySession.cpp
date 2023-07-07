/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "EmptySession.h"

namespace igl {
namespace shell {

void EmptySession::initialize() noexcept {
  getPlatform().getDevice();
}

void EmptySession::update(igl::SurfaceTextures surfaceTextures) noexcept {}

} // namespace shell
} // namespace igl
