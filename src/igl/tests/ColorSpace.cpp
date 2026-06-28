/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/ColorSpace.h>

namespace igl::tests {

TEST(ColorSpaceTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::SRGBLinear), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::SRGBNonlinear), 1u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::DisplayP3Nonlinear), 2u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::ExtendedSRGBLinear), 3u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::DisplayP3Linear), 4u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::DCIP3Nonlinear), 5u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT709Linear), 6u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT709Nonlinear), 7u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT2020Linear), 8u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::HDR10St2084), 9u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::DolbyVision), 10u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::HDR10HLG), 11u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::AdobeRGBLinear), 12u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::AdobeRGBNonlinear), 13u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::PassThrough), 14u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::ExtendedSRGBNonlinear), 15u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::DisplayNativeAMD), 16u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT601Nonlinear), 17u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT2020Nonlinear), 18u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT2100HLGNonlinear), 19u);
  EXPECT_EQ(static_cast<uint8_t>(ColorSpace::BT2100PQNonlinear), 20u);
}

TEST(ColorSpaceTest, colorSpaceToString) {
  ASSERT_STREQ(colorSpaceToString(ColorSpace::SRGBLinear), "SRGBLinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::SRGBNonlinear), "SRGBNonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DisplayP3Nonlinear), "DisplayP3Nonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::ExtendedSRGBLinear), "ExtendedSRGBLinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DisplayP3Linear), "DisplayP3Linear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DCIP3Nonlinear), "DCIP3Nonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT709Linear), "BT709Linear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT709Nonlinear), "BT709Nonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2020Linear), "BT2020Linear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::HDR10St2084), "HDR10St2084");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DolbyVision), "DolbyVision");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::HDR10HLG), "HDR10HLG");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::AdobeRGBLinear), "AdobeRGBLinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::AdobeRGBNonlinear), "AdobeRGBNonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::PassThrough), "PassThrough");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::ExtendedSRGBNonlinear), "ExtendedSRGBNonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DisplayNativeAMD), "DisplayNativeAMD");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT601Nonlinear), "BT601Nonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2020Nonlinear), "BT2020Nonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2100HLGNonlinear), "BT2100HLGNonlinear");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2100PQNonlinear), "BT2100PQNonlinear");
}

} // namespace igl::tests
