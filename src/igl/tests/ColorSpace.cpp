/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/ColorSpace.h>

namespace igl::tests {

TEST(ColorSpaceTest, colorSpaceToString) {
  ASSERT_STREQ(colorSpaceToString(ColorSpace::SRGB_LINEAR), "SRGB_LINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::SRGB_NONLINEAR), "SRGB_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DISPLAY_P3_NONLINEAR), "DISPLAY_P3_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::EXTENDED_SRGB_LINEAR), "EXTENDED_SRGB_LINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DISPLAY_P3_LINEAR), "DISPLAY_P3_LINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DCI_P3_NONLINEAR), "DCI_P3_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT709_LINEAR), "BT709_LINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT709_NONLINEAR), "BT709_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2020_LINEAR), "BT2020_LINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::HDR10_ST2084), "HDR10_ST2084");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DOLBYVISION), "DOLBYVISION");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::HDR10_HLG), "HDR10_HLG");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::ADOBERGB_LINEAR), "ADOBERGB_LINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::ADOBERGB_NONLINEAR), "ADOBERGB_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::PASS_THROUGH), "PASS_THROUGH");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::EXTENDED_SRGB_NONLINEAR), "EXTENDED_SRGB_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::DISPLAY_NATIVE_AMD), "DISPLAY_NATIVE_AMD");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT601_NONLINEAR), "BT601_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2020_NONLINEAR), "BT2020_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2100_HLG_NONLINEAR), "BT2100_HLG_NONLINEAR");
  ASSERT_STREQ(colorSpaceToString(ColorSpace::BT2100_PQ_NONLINEAR), "BT2100_PQ_NONLINEAR");
}

} // namespace igl::tests
