/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

inline TextureFormat sRGBToLinear(TextureFormat format) {
  FOLLY_PUSH_WARNING
  FOLLY_CLANG_DISABLE_WARNING("-Wswitch-enum")
  switch (format) {
  case TextureFormat::RGBA_SRGB:
    return TextureFormat::RGBA_UNorm8;
  case TextureFormat::BGRA_SRGB:
    return TextureFormat::BGRA_UNorm8;
  // @fb-only
    // @fb-only
  // @fb-only
    // @fb-only
  default:
    break;
  }
  FOLLY_POP_WARNING
  IGL_UNREACHABLE_RETURN(TextureFormat::RGBA_UNorm8)
}

inline TextureFormat linearTosRGB(TextureFormat format) {
  FOLLY_PUSH_WARNING
  FOLLY_CLANG_DISABLE_WARNING("-Wswitch-enum")
  switch (format) {
  case TextureFormat::RGBA_UNorm8:
    return TextureFormat::RGBA_SRGB;
  case TextureFormat::BGRA_UNorm8:
    return TextureFormat::BGRA_SRGB;
  // @fb-only
    // @fb-only
  // @fb-only
    // @fb-only
  default:
    break;
  }
  FOLLY_POP_WARNING
  IGL_UNREACHABLE_RETURN(TextureFormat::RGBA_SRGB)
}

inline TextureFormat BgraToRgba(TextureFormat format) {
  if (format == TextureFormat::BGRA_UNorm8) {
    return TextureFormat::RGBA_UNorm8;
  } else if (format == TextureFormat::BGRA_SRGB) {
    return TextureFormat::RGBA_SRGB;
  }
  return format;
}

inline TextureFormat RgbaToBgra(TextureFormat format) {
  if (format == TextureFormat::RGBA_UNorm8) {
    return TextureFormat::BGRA_UNorm8;
  } else if (format == TextureFormat::RGBA_SRGB) {
    return TextureFormat::BGRA_SRGB;
  }
  return format;
}

} // namespace igl
