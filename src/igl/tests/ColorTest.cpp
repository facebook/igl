/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Color.h>

#include <igl/ColorSpace.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------

TEST(ColorTest, ThreeArgConstructorDefaultsAlphaToOne) {
  constexpr Color c(0.1f, 0.2f, 0.3f);
  EXPECT_FLOAT_EQ(c.r, 0.1f);
  EXPECT_FLOAT_EQ(c.g, 0.2f);
  EXPECT_FLOAT_EQ(c.b, 0.3f);
  EXPECT_FLOAT_EQ(c.a, 1.0f);
}

TEST(ColorTest, FourArgConstructor) {
  constexpr Color c(0.5f, 0.6f, 0.7f, 0.8f);
  EXPECT_FLOAT_EQ(c.r, 0.5f);
  EXPECT_FLOAT_EQ(c.g, 0.6f);
  EXPECT_FLOAT_EQ(c.b, 0.7f);
  EXPECT_FLOAT_EQ(c.a, 0.8f);
}

TEST(ColorTest, ZeroColor) {
  constexpr Color c(0.0f, 0.0f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(c.r, 0.0f);
  EXPECT_FLOAT_EQ(c.g, 0.0f);
  EXPECT_FLOAT_EQ(c.b, 0.0f);
  EXPECT_FLOAT_EQ(c.a, 0.0f);
}

TEST(ColorTest, ToFloatPtrPointsToR) {
  const Color c(0.1f, 0.2f, 0.3f, 0.4f);
  const float* ptr = c.toFloatPtr();
  ASSERT_NE(ptr, nullptr);
  EXPECT_FLOAT_EQ(ptr[0], 0.1f);
  EXPECT_FLOAT_EQ(ptr[1], 0.2f);
  EXPECT_FLOAT_EQ(ptr[2], 0.3f);
  EXPECT_FLOAT_EQ(ptr[3], 0.4f);
}

TEST(ColorTest, ConstexprConstruction) {
  constexpr Color c3(1.0f, 0.0f, 0.0f);
  constexpr Color c4(1.0f, 0.0f, 0.0f, 0.5f);
  EXPECT_FLOAT_EQ(c3.a, 1.0f);
  EXPECT_FLOAT_EQ(c4.a, 0.5f);
}

// ---------------------------------------------------------------------------
// ColorSpace
// ---------------------------------------------------------------------------

TEST(ColorSpaceTest, ToStringCoversAllValues) {
  EXPECT_STREQ(colorSpaceToString(ColorSpace::SRGBLinear), "SRGBLinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::SRGBNonlinear), "SRGBNonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::DisplayP3Nonlinear), "DisplayP3Nonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::ExtendedSRGBLinear), "ExtendedSRGBLinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::DisplayP3Linear), "DisplayP3Linear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::DCIP3Nonlinear), "DCIP3Nonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT709Linear), "BT709Linear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT709Nonlinear), "BT709Nonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT2020Linear), "BT2020Linear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::HDR10St2084), "HDR10St2084");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::DolbyVision), "DolbyVision");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::HDR10HLG), "HDR10HLG");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::AdobeRGBLinear), "AdobeRGBLinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::AdobeRGBNonlinear), "AdobeRGBNonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::PassThrough), "PassThrough");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::ExtendedSRGBNonlinear), "ExtendedSRGBNonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::DisplayNativeAMD), "DisplayNativeAMD");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT601Nonlinear), "BT601Nonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT2020Nonlinear), "BT2020Nonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT2100HLGNonlinear), "BT2100HLGNonlinear");
  EXPECT_STREQ(colorSpaceToString(ColorSpace::BT2100PQNonlinear), "BT2100PQNonlinear");
}

TEST(ColorSpaceTest, EnumValuesAreDistinct) {
  EXPECT_NE(static_cast<uint8_t>(ColorSpace::SRGBLinear),
            static_cast<uint8_t>(ColorSpace::SRGBNonlinear));
  EXPECT_NE(static_cast<uint8_t>(ColorSpace::BT2020Linear),
            static_cast<uint8_t>(ColorSpace::BT2020Nonlinear));
  EXPECT_NE(static_cast<uint8_t>(ColorSpace::HDR10St2084),
            static_cast<uint8_t>(ColorSpace::HDR10HLG));
}

TEST(ColorSpaceTest, FirstEnumValueIsZero) {
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::SRGBLinear), 0u);
}

} // namespace igl::tests
