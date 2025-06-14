/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/Common.h>
#include <igl/Macros.h>

namespace igl {

/**
 *
 * The list of different color space commonly seen in graphics.
 *
 */
enum class ColorSpace : uint8_t {
  SRGB_LINEAR,
  SRGB_NONLINEAR,
  DISPLAY_P3_NONLINEAR,
  EXTENDED_SRGB_LINEAR,
  DISPLAY_P3_LINEAR,
  DCI_P3_NONLINEAR,
  BT709_LINEAR,
  BT709_NONLINEAR,
  BT2020_LINEAR,
  HDR10_ST2084,
  DOLBYVISION,
  HDR10_HLG,
  ADOBERGB_LINEAR,
  ADOBERGB_NONLINEAR,
  PASS_THROUGH,
  EXTENDED_SRGB_NONLINEAR,
  DISPLAY_NATIVE_AMD,
  BT601_NONLINEAR,
  BT2020_NONLINEAR,
  BT2100_HLG_NONLINEAR,
  BT2100_PQ_NONLINEAR
};

/**
 * @brief Converts color space to literal string
 *
 * @param colorSpace ColorSpace of the texture/framebuffer.
 * @return Literal C-style string containing the color space name
 */
inline const char* IGL_NONNULL colorSpaceToString(ColorSpace colorSpace) {
  switch (colorSpace) {
    IGL_ENUM_TO_STRING(ColorSpace, SRGB_LINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, SRGB_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, DISPLAY_P3_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, EXTENDED_SRGB_LINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, DISPLAY_P3_LINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, DCI_P3_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, BT709_LINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, BT709_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, BT2020_LINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, HDR10_ST2084)
    IGL_ENUM_TO_STRING(ColorSpace, DOLBYVISION)
    IGL_ENUM_TO_STRING(ColorSpace, HDR10_HLG)
    IGL_ENUM_TO_STRING(ColorSpace, ADOBERGB_LINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, ADOBERGB_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, PASS_THROUGH)
    IGL_ENUM_TO_STRING(ColorSpace, EXTENDED_SRGB_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, DISPLAY_NATIVE_AMD)
    IGL_ENUM_TO_STRING(ColorSpace, BT601_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, BT2020_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, BT2100_HLG_NONLINEAR)
    IGL_ENUM_TO_STRING(ColorSpace, BT2100_PQ_NONLINEAR)
  default:
    break;
  }

  IGL_UNREACHABLE_RETURN("unknown color space")
}

} // namespace igl
