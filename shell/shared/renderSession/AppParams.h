/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <shell/shared/renderSession/DepthParams.h>

namespace igl::shell {
struct AppParams {
  DepthParams depthParams;
  bool exitRequested = false;
  float sizeX = 1.f;
  float sizeY = 1.f;
};
} // namespace igl::shell
