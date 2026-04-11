/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/Common.h> // IWYU pragma: keep
#include <igl/Macros.h>

namespace igl {

/**
 *
 * The list of different color space commonly seen in graphics.
 *
 */
enum class ColorSpace : uint8_t {
  SRGBLinear,
  SRGBNonlinear,
  DisplayP3Nonlinear,
  ExtendedSRGBLinear,
  DisplayP3Linear,
  DCIP3Nonlinear,
  BT709Linear,
  BT709Nonlinear,
  BT2020Linear,
  HDR10St2084,
  DolbyVision,
  HDR10HLG,
  AdobeRGBLinear,
  AdobeRGBNonlinear,
  PassThrough,
  ExtendedSRGBNonlinear,
  DisplayNativeAMD,
  BT601Nonlinear,
  BT2020Nonlinear,
  BT2100HLGNonlinear,
  BT2100PQNonlinear
};

/**
 * @brief Converts color space to literal string
 *
 * @param colorSpace ColorSpace of the texture/framebuffer.
 * @return Literal C-style string containing the color space name
 */
inline const char* IGL_NONNULL colorSpaceToString(ColorSpace colorSpace) {
  switch (colorSpace) {
    IGL_ENUM_TO_STRING(ColorSpace, SRGBLinear)
    IGL_ENUM_TO_STRING(ColorSpace, SRGBNonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, DisplayP3Nonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, ExtendedSRGBLinear)
    IGL_ENUM_TO_STRING(ColorSpace, DisplayP3Linear)
    IGL_ENUM_TO_STRING(ColorSpace, DCIP3Nonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, BT709Linear)
    IGL_ENUM_TO_STRING(ColorSpace, BT709Nonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, BT2020Linear)
    IGL_ENUM_TO_STRING(ColorSpace, HDR10St2084)
    IGL_ENUM_TO_STRING(ColorSpace, DolbyVision)
    IGL_ENUM_TO_STRING(ColorSpace, HDR10HLG)
    IGL_ENUM_TO_STRING(ColorSpace, AdobeRGBLinear)
    IGL_ENUM_TO_STRING(ColorSpace, AdobeRGBNonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, PassThrough)
    IGL_ENUM_TO_STRING(ColorSpace, ExtendedSRGBNonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, DisplayNativeAMD)
    IGL_ENUM_TO_STRING(ColorSpace, BT601Nonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, BT2020Nonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, BT2100HLGNonlinear)
    IGL_ENUM_TO_STRING(ColorSpace, BT2100PQNonlinear)
  default:
    break;
  }

  IGL_UNREACHABLE_RETURN("unknown color space")
}

} // namespace igl
