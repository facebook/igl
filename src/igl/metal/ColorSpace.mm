/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ColorSpace.h>

namespace igl::metal {
CGColorSpaceRef colorSpaceToCGColorSpace(igl::ColorSpace colorSpace) {
  switch (colorSpace) {
  case ColorSpace::SRGB_LINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
  case ColorSpace::SRGB_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  case ColorSpace::DISPLAY_P3_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
  case ColorSpace::DISPLAY_P3_LINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceLinearDisplayP3);
  case ColorSpace::EXTENDED_SRGB_LINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
  case ColorSpace::DCI_P3_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceDCIP3);
  case ColorSpace::BT709_LINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
  case ColorSpace::BT709_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceITUR_709);
  case ColorSpace::BT2020_LINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceLinearITUR_2020);
  case ColorSpace::ADOBERGB_LINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998);
  case ColorSpace::ADOBERGB_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998);
  case ColorSpace::PASS_THROUGH:
    return nil;
  case ColorSpace::EXTENDED_SRGB_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
  case ColorSpace::BT2020_NONLINEAR:
    return CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
  case ColorSpace::HDR10_ST2084:
  case ColorSpace::DOLBYVISION:
  case ColorSpace::HDR10_HLG:
  case ColorSpace::DISPLAY_NATIVE_AMD:
  case ColorSpace::BT601_NONLINEAR:
  case ColorSpace::BT2100_HLG_NONLINEAR:
  case ColorSpace::BT2100_PQ_NONLINEAR:
  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  }
  IGL_UNREACHABLE_RETURN(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
}
} // namespace igl::metal
