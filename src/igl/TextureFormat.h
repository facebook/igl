/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <cstdint>

namespace igl {
/**
 *
 * The IGL format name specification is as follows:
 *
 *  There shall be 3 naming format base types: those for component array
 *  formats (type A); those for compressed formats (type C); and those for
 *  packed component formats (type P). With type A formats, color component
 *  order does not change with endianness. Each format name shall begin with
 *  TextureFormat::, followed by a component label (from the Component Label
 *  list below) for each component in the order that the component(s) occur
 *  in the format, except for non-linear color formats where the first
 *  letter shall be 'S'. For type P formats, each component label is
 *  followed by the number of bits that represent it in the fundamental
 *  data type used by the format.
 *
 *  Following the listing of the component labels shall be an underscore; a
 *  compression type followed by an underscore for Type C formats only; a
 *  storage type from the list below; and a bit width for type A formats,
 *  which is the bit width for each array element.
 *
 *  If a format is vendor-specific, then a "_vendor" post fix may be
 *  added to the type
 *
 *
 *  ----------    Format Base Type A: Array ----------
 *  TextureFormat::[component list]_[storage type][array element bit width][_vendor]
 *
 *  Examples:
 *  TextureFormat::A_SNorm8 - uchar[i] = A
 *  TextureFormat::RGBA_SNorm16 - ushort[i * 4 + 0] = R, ushort[i * 4 + 1] = G,
 *                                ushort[i * 4 + 2] = B, ushort[i * 4 + 3] = A
 *  TextureFormat::Z_UNorm32 - int32[i] = Z
 *
 *
 *  ----------    Format Base Type C: Compressed ----------
 *  TextureFormat::[component list#][_*][compression type][_*][block size][_*][storage type#]
 *    # where required
 *
 *  Examples:
 *  TextureFormat::RGB_ETC1
 *  TextureFormat::RGBA_ASTC_4x4
 *  TextureFormat::RGB_PVRTC_2BPPV1
 *
 *
 *  ----------    Format Base Type P: Packed  ----------
 *  TextureFormat::[[component list,bit width][storage type#][_]][_][storage type##][_storage
 * order###][_vendor#]
 *    # when type differs between component
 *    ## when type applies to all components
 *    ### when storage order is hardware independent
 *
 *  Examples:
 *  TextureFormat::A8B8G8R8_UNorm
 *  TextureFormat::R5G6B5_UNorm
 *  TextureFormat::B4G4R4X4_UNorm
 *  TextureFormat::Z32_F_S8X24_UInt
 *  TextureFormat::R10G10B10A2_UInt
 *  TextureFormat::R9G9B9E5_F
 *  TextureFormat::BGRA_UNorm8_Rev
 *
 *
 *  ----------    Component Labels: ----------
 *  A - Alpha
 *  B - Blue
 *  G - Green
 *  I - Intensity
 *  L - Luminance
 *  R - Red
 *  S - Stencil (when not followed by RGB or RGBA)
 *  S - non-linear types (when followed by RGB or RGBA)
 *  X - Packing bits
 *  Z - Depth
 *
 *  ----------    Storage Types: ----------
 *  F: float
 *  SInt: Signed Integer
 *  UInt: Unsigned Integer
 *  SNorm: Signed Normalized Integer/Byte
 *  UNorm: Unsigned Normalized Integer/Byte
 *
 *  ----------    Type C Compression Types: Additional Info ----------
 *  ETC1 - No other information required
 *  ETC2 - No other information required
 *  ASTC - Block size shall be given
 *  PVRTC - Block size shall be given
 *
 */

enum class TextureFormat : uint8_t {
  Invalid = 0,

  // 8 bpp
  A_UNorm8,
  L_UNorm8,
  R_UNorm8,

  // 16 bpp
  R_F16,
  R_UInt16,
  R_UNorm16,
  B5G5R5A1_UNorm,
  B5G6R5_UNorm,
  ABGR_UNorm4, // NA on GLES
  LA_UNorm8,
  RG_UNorm8,
  R4G2B2_UNorm_Apple,
  R4G2B2_UNorm_Rev_Apple,
  R5G5B5A1_UNorm,

  // 24 bpp
  RGBX_UNorm8,

  // 32 bpp
  RGBA_UNorm8,
  BGRA_UNorm8,
  BGRA_UNorm8_Rev,
  RGBA_SRGB,
  BGRA_SRGB,
  RG_F16,
  RG_UInt16,
  RG_UNorm16,
  RGB10_A2_UNorm_Rev,
  RGB10_A2_Uint_Rev,
  BGR10_A2_Unorm,
  R_F32,
  // 48 bpp
  RGB_F16,

  // 64 bpp
  RGBA_F16,
  RG_F32,

  // 96 bpp
  RGB_F32,

  // 128 bpp
  RGBA_UInt32,
  RGBA_F32,

  // Compressed
  RGBA_ASTC_4x4,
  SRGB8_A8_ASTC_4x4,
  RGBA_ASTC_5x4,
  SRGB8_A8_ASTC_5x4,
  RGBA_ASTC_5x5,
  SRGB8_A8_ASTC_5x5,
  RGBA_ASTC_6x5,
  SRGB8_A8_ASTC_6x5,
  RGBA_ASTC_6x6,
  SRGB8_A8_ASTC_6x6,
  RGBA_ASTC_8x5,
  SRGB8_A8_ASTC_8x5,
  RGBA_ASTC_8x6,
  SRGB8_A8_ASTC_8x6,
  RGBA_ASTC_8x8,
  SRGB8_A8_ASTC_8x8,
  RGBA_ASTC_10x5,
  SRGB8_A8_ASTC_10x5,
  RGBA_ASTC_10x6,
  SRGB8_A8_ASTC_10x6,
  RGBA_ASTC_10x8,
  SRGB8_A8_ASTC_10x8,
  RGBA_ASTC_10x10,
  SRGB8_A8_ASTC_10x10,
  RGBA_ASTC_12x10,
  SRGB8_A8_ASTC_12x10,
  RGBA_ASTC_12x12,
  SRGB8_A8_ASTC_12x12,
  RGBA_PVRTC_2BPPV1,
  RGB_PVRTC_2BPPV1,
  RGBA_PVRTC_4BPPV1,
  RGB_PVRTC_4BPPV1,
  RGB8_ETC1,
  RGB8_ETC2,
  SRGB8_ETC2,
  RGB8_Punchthrough_A1_ETC2,
  SRGB8_Punchthrough_A1_ETC2,
  RGBA8_EAC_ETC2,
  SRGB8_A8_EAC_ETC2,
  RG_EAC_UNorm,
  RG_EAC_SNorm,
  R_EAC_UNorm,
  R_EAC_SNorm,
  RGBA_BC7_UNORM_4x4, // block compression
  RGBA_BC7_SRGB_4x4, // block compression

  // Depth and Stencil formats
  Z_UNorm16, // NA on iOS/Metal but works on iOS GLES. The client has to account for
             // this!
  Z_UNorm24,
  Z_UNorm32, // NA on iOS/GLES but works on iOS Metal. The client has to account for
             // this!
  S8_UInt_Z24_UNorm, // NA on iOS
  S8_UInt_Z32_UNorm, // NA on iOS/GLES but works on iOS Metal. The client has to
                     // account for this!
  S_UInt8,

  YUV_NV12, // Semi-planar 8-bit YUV 4:2:0 NV12; 2 planes in a single image
  YUV_420p, // Tri-planar  8-bit YUV 4:2:0;      3 planes in a single image
};
} // namespace igl
