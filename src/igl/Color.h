/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Null.h>

namespace igl {

struct Color {
  float r;
  float g;
  float b;
  float a;

  constexpr Color(float r, float g, float b) : r(r), g(g), b(b), a(1.0f) {}
  constexpr Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

  [[nodiscard]] const float* IGL_NONNULL toFloatPtr() const {
    return &r;
  }
};
} // namespace igl
