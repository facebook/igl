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
            colorSpaceToCGColorSpace(ColorSpace::SRGBLinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::SRGBNonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3),
            colorSpaceToCGColorSpace(ColorSpace::DisplayP3Nonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB),
            colorSpaceToCGColorSpace(ColorSpace::ExtendedSRGBLinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceDCIP3),
            colorSpaceToCGColorSpace(ColorSpace::DCIP3Nonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT709Linear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceITUR_709),
            colorSpaceToCGColorSpace(ColorSpace::BT709Nonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998),
            colorSpaceToCGColorSpace(ColorSpace::AdobeRGBLinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998),
            colorSpaceToCGColorSpace(ColorSpace::AdobeRGBNonlinear));

  ASSERT_EQ(nil, colorSpaceToCGColorSpace(ColorSpace::PassThrough));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB),
            colorSpaceToCGColorSpace(ColorSpace::ExtendedSRGBNonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020),
            colorSpaceToCGColorSpace(ColorSpace::BT2020Nonlinear));

  if (@available(macOS 12.0, iOS 15.0, *)) {
    ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearDisplayP3),
              colorSpaceToCGColorSpace(ColorSpace::DisplayP3Linear));

    ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceLinearITUR_2020),
              colorSpaceToCGColorSpace(ColorSpace::BT2020Linear));
  }

  // can't test unsupported cases
  /*ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::HDR10St2084));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::DolbyVision));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::HDR10HLG));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::DisplayNativeAMD));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT601Nonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT2100HLGNonlinear));

  ASSERT_EQ(CGColorSpaceCreateWithName(kCGColorSpaceSRGB),
            colorSpaceToCGColorSpace(ColorSpace::BT2100PQNonlinear));*/
}

} // namespace igl::metal::tests
