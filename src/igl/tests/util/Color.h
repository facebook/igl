/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <glm/gtc/color_space.hpp>

namespace igl::tests::util {
// To avoid mix matching gamma, this defines a constant that everyone can use until it can be
// configured on a per project basis.
constexpr double kDefaultGamma = 2.4;

// force double precision color conversion to not lose precision
template<glm::length_t L, typename T, glm::qualifier Q>
glm::vec<L, T, Q> convertSRGBToLinear(const glm::vec<L, T, Q>& nonLinearColor,
                                      double gamma = kDefaultGamma) {
  return glm::vec<L, T, Q>(glm::convertSRGBToLinear(glm::vec<L, double, Q>(nonLinearColor), gamma));
}

// force double precision color conversion to not lose precision
template<glm::length_t L, typename T, glm::qualifier Q>
glm::vec<L, T, Q> convertLinearToSRGB(const glm::vec<L, T, Q>& linearColor,
                                      double gamma = kDefaultGamma) {
  return glm::vec<L, T, Q>(glm::convertLinearToSRGB(glm::vec<L, double, Q>(linearColor), gamma));
}
} // namespace igl::tests::util
