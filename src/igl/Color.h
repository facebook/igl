/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Null.h>

namespace igl {

/**
 * @brief struct to represent a linear color value.
 */
struct Color {
  float r;
  float g;
  float b;
  float a;

  /**
   * @brief Constructor ingesting red, green, blue. Alpha is assumed to be 1.0f.
   *
   * @param red - red channel
   * @param green - green channel
   * @param blue - blue channel
   */
  constexpr Color(float red, float green, float blue) : r(red), g(green), b(blue), a(1.0f) {}

  /**
   * @brief Constructor ingesting red, green, blue and alpha.
   *
   * @param red - red channel
   * @param green - green channel
   * @param blue - blue channel
   * @param alpha - alpha channel
   */
  constexpr Color(float red, float green, float blue, float alpha) :
    r(red), g(green), b(blue), a(alpha) {}

  [[nodiscard]] const float* IGL_NONNULL toFloatPtr() const {
    return &r;
  }
};
} // namespace igl
