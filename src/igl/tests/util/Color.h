/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <cstdint>
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

/**
 * @brief struct to represent a 32bits sRGB color value. It is assumed that the rgb colors are gamma
 * compressed using the sRGB transfer function and the alpha is linear.
 */
struct sRGBColor {
  uint8_t r{};
  uint8_t g{};
  uint8_t b{};
  uint8_t a{};

  /**
   * @brief Constructor ingesting red, green, blue. alpha is assumed to be 255.
   * @remark the red green and blue are assumed to be gamma compressed using the sRGB transfer
   * function.
   *
   * @param red - red channel
   * @param green - green channel
   * @param blue - blue channel
   */
  constexpr sRGBColor(uint8_t red, uint8_t green, uint8_t blue) :
    r(red), g(green), b(blue), a(255) {}

  /**
   * @brief Constructor ingesting red, green, blue, alpha.
   * @remark the red green and blue are assumed to be gamma compressed using the sRGB transfer
   * function and the alpha is linear..
   *
   * @param red - red channel
   * @param green - green channel
   * @param blue - blue channel
   * @param alpha - alpha channel
   */
  constexpr sRGBColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) :
    r(red), g(green), b(blue), a(alpha) {}

  /**
   * @brief Constructor ingesting red, green, blue, alpha.
   *
   * @param rgba - 32bits integer containing concatenated 8 bits for red, green, blue and alpha.
   * @remark the red green and blue are assumed to be gamma compressed using the sRGB transfer
   * function and the alpha is linear.
   */
  sRGBColor(uint32_t rgba) {
    fromRGBA32(rgba);
  }

  /**
   * @brief Encode the rgba values into a single 32bits integer.
   * @return 32bits integer containing the rgba values.
   * @remark the red green and blue are assumed to be gamma compressed using the sRGB transfer
   * function and the alpha is linear.
   */
  [[nodiscard]] constexpr uint32_t toRGBA32() const {
    return (r << 24) | (g << 16) | (b << 8) | a;
  }

  /**
   * @brief Decode the rgba values into a single 32bits integer.
   * @param rgba - 32bits integer containing the rgba values.
   * @remark the rgba are assumed to be gamma compressed using the sRGB transfer function and the
   * alpha is linear.
   */
  constexpr void fromRGBA32(uint32_t rgba) {
    r = (rgba >> 24);
    g = (rgba >> 16) & 0xff;
    b = (rgba >> 8) & 0xff;
    a = (rgba >> 0) & 0xff;
  }
};
} // namespace igl::tests::util
