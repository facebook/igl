/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/metal/ColorSpace.h>

namespace igl::metal::tests {
TEST(ColorSpaceTest, colorSpaceToCGColorSpace) {
  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB),
            colorSpaceToCGColorSpace(ColorSpace::SRGB_LINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::SRGB_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3),
            colorSpaceToCGColorSpace(ColorSpace::DISPLAY_P3_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB),
            colorSpaceToCGColorSpace(ColorSpace::EXTENDED_SRGB_LINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceDCIP3),
            colorSpaceToCGColorSpace(ColorSpace::DCI_P3_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT709_LINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceITUR_709),
            colorSpaceToCGColorSpace(ColorSpace::BT709_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998),
            colorSpaceToCGColorSpace(ColorSpace::ADOBERGB_LINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998),
            colorSpaceToCGColorSpace(ColorSpace::ADOBERGB_NONLINEAR));

  ASSERT_EQ(nil, colorSpaceToCGColorSpace(ColorSpace::PASS_THROUGH));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB),
            colorSpaceToCGColorSpace(ColorSpace::EXTENDED_SRGB_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020),
            colorSpaceToCGColorSpace(ColorSpace::BT2020_NONLINEAR));

  if (@available(macOS 12.0, iOS 15.0, *)) {
    ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearDisplayP3),
              colorSpaceToCGColorSpace(ColorSpace::DISPLAY_P3_LINEAR));

    ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearITUR_2020),
              colorSpaceToCGColorSpace(ColorSpace::BT2020_LINEAR));
  }

  // can't test unsupported cases
  /*ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::HDR10_ST2084));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::DOLBYVISION));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::HDR10_HLG));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::DISPLAY_NATIVE_AMD));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT601_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT2100_HLG_NONLINEAR));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT2100_PQ_NONLINEAR));*/
}

} // namespace igl::metal::tests
