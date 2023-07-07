/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::shell {
struct Fov {
  float angleLeft = .0f;
  float angleRight = .0f;
  float angleUp = .0f;
  float angleDown = .0f;
  Fov() = default;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  Fov(float left, float right, float up, float down) :
    angleLeft(left), angleRight(right), angleUp(up), angleDown(down) {}
};
} // namespace igl::shell
