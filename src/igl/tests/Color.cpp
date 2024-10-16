/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/Color.h"
#include <gtest/gtest.h>
#include <igl/Color.h>

namespace igl::tests {

TEST(ColorTest, ctor) {
  const Color testColor(1.0f, 0.5f, 0.0f);
  ASSERT_EQ(testColor.r, 1.0f);
  ASSERT_EQ(testColor.g, 0.5f);
  ASSERT_EQ(testColor.b, 0.0f);
  ASSERT_EQ(testColor.a, 1.0f);

  const Color testColor2(1.0f, 0.5f, 0.0f, 1.0f);

  ASSERT_EQ(testColor2.r, 1.0f);
  ASSERT_EQ(testColor2.g, 0.5f);
  ASSERT_EQ(testColor2.b, 0.0f);
  ASSERT_EQ(testColor2.a, 1.0f);

  const auto* floatPtr = testColor.toFloatPtr();
  ASSERT_EQ(floatPtr[0], 1.0f);
  ASSERT_EQ(floatPtr[1], 0.5f);
  ASSERT_EQ(floatPtr[2], 0.0f);
  ASSERT_EQ(floatPtr[3], 1.0f);
}

TEST(sRGBColorTest, ctor) {
  const util::sRGBColor testColor(255, 128, 0);
  ASSERT_EQ(testColor.r, 255);
  ASSERT_EQ(testColor.g, 128);
  ASSERT_EQ(testColor.b, 0);
  ASSERT_EQ(testColor.a, 255);

  const util::sRGBColor testColor2(255, 128, 0, 255);

  ASSERT_EQ(testColor2.r, 255);
  ASSERT_EQ(testColor2.g, 128);
  ASSERT_EQ(testColor2.b, 0);
  ASSERT_EQ(testColor2.a, 255);
}

TEST(sRGBColorTest, toRGBA32) {
  const util::sRGBColor testColor(255, 128, 0);
  ASSERT_EQ(testColor.toRGBA32(), 0xff8000ff);
}

TEST(sRGBColorTest, fromRGBA32) {
  const util::sRGBColor testColor(0xff8000ff);
  ASSERT_EQ(testColor.r, 255);
  ASSERT_EQ(testColor.g, 128);
  ASSERT_EQ(testColor.b, 0);
  ASSERT_EQ(testColor.a, 255);
}
} // namespace igl::tests
