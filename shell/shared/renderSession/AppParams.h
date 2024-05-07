/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <cstdint>
#include <functional>
#include <shell/shared/renderSession/DepthParams.h>
#include <shell/shared/renderSession/QuadLayerParams.h>

namespace igl::shell {
using QuadLayerParamsGetter = std::function<QuadLayerParams()>;
using PassthroughGetter = std::function<bool()>;

struct AppParams {
  DepthParams depthParams;
  bool exitRequested = false;
  float sizeX = 1.f;
  float sizeY = 1.f;
  QuadLayerParamsGetter quadLayerParamsGetter;
  PassthroughGetter passthroughGetter;
};
} // namespace igl::shell
