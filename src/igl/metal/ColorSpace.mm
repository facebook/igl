/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ColorSpace.h>

namespace igl::metal {
CGColorSpaceRef colorSpaceToCGColorSpace(ColorSpace colorSpace) {
  switch (colorSpace) {
  case ColorSpace::SRGBLinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
  case ColorSpace::SRGBNonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  case ColorSpace::DisplayP3Nonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
  case ColorSpace::ExtendedSRGBLinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
  case ColorSpace::DCIP3Nonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceDCIP3);
  case ColorSpace::BT709Linear:
    return CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
  case ColorSpace::BT709Nonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceITUR_709);
  case ColorSpace::AdobeRGBLinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998);
  case ColorSpace::AdobeRGBNonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998);
  case ColorSpace::PassThrough:
    return nil;
  case ColorSpace::ExtendedSRGBNonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
  case ColorSpace::BT2020Nonlinear:
    return CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
  case ColorSpace::DisplayP3Linear:
    if (@available(macOS 12.0, iOS 15.0, *)) {
      return CGColorSpaceCreateWithName(kCGColorSpaceLinearDisplayP3);
    }
    [[fallthrough]];
  case ColorSpace::BT2020Linear:
    if (@available(macOS 12.0, iOS 15.0, *)) {
      return CGColorSpaceCreateWithName(kCGColorSpaceLinearITUR_2020);
    }
    [[fallthrough]];
  case ColorSpace::HDR10St2084:
  case ColorSpace::DolbyVision:
  case ColorSpace::HDR10HLG:
  case ColorSpace::DisplayNativeAMD:
  case ColorSpace::BT601Nonlinear:
  case ColorSpace::BT2100HLGNonlinear:
  case ColorSpace::BT2100PQNonlinear:
  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  }
  IGL_UNREACHABLE_RETURN(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
}
} // namespace igl::metal
