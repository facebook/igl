/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/opengl/util/TextureFormat.h>

// The GL_* constants below are #defined privately inside opengl/util/TextureFormat.cpp
// and are not exposed via any IGL public header. Redefine the ones exercised by the
// tests with the exact same hex values used in the production source.
// NOLINTBEGIN(readability-identifier-naming)
#define GL_ALPHA 0x1906
#define GL_ALPHA8 0x803C
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_BGRA8_EXT 0x93A1
#define GL_COMPRESSED_R11_EAC 0x9270
#define GL_COMPRESSED_RG11_EAC 0x9272
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG 0x8C01
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG 0x8C00
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR 0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR 0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR 0x93BA
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR 0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR 0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR 0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR 0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8C03
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8C02
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define GL_COMPRESSED_SIGNED_R11_EAC 0x9271
#define GL_COMPRESSED_SIGNED_RG11_EAC 0x9273
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR 0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR 0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR 0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR 0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR 0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR 0x93DD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR 0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR 0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR 0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR 0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9277
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT16 0x81a5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_DEPTH_STENCIL 0x84F9
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_ETC1_RGB8_OES 0x8D64
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV 0x8DAD
#define GL_HALF_FLOAT_OES 0x8D61
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190a
#define GL_LUMINANCE8 0x8040
#define GL_LUMINANCE8_ALPHA8 0x8045
#define GL_R16 0x822A
#define GL_R16F 0x822D
#define GL_R16UI 0x8234
#define GL_R32UI 0x8236
#define GL_R32F 0x822E
#define GL_RED 0x1903
#define GL_RG 0x8227
#define GL_RG16 0x822C
#define GL_RG16F 0x822F
#define GL_RG16UI 0x823a
#define GL_RG32F 0x8230
#define GL_RGB 0x1907
#define GL_RGBA16 0x805B
#define GL_RGB_RAW_422_APPLE 0x8A51
#define GL_RGB10_A2 0x8059
#define GL_RGB10_A2UI 0x906f
#define GL_RGB16F 0x881B
#define GL_RGB32F 0x8815
#define GL_RGB5_A1 0x8057
#define GL_RGBA 0x1908
#define GL_RGBA16F 0x881A
#define GL_RGBA32F 0x8814
#define GL_RGBA32UI 0x8d70
#define GL_RGBA4 0x8056
#define GL_RGBA8 0x8058
#define GL_SRGB_ALPHA 0x8c42
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_STENCIL_INDEX 0x1901
#define GL_STENCIL_INDEX8 0x8d48
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_UNSIGNED_INT_24_8 0x84fa
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_8_8_APPLE 0x85BA
#define GL_UNSIGNED_SHORT_8_8_REV_APPLE 0x85BB
// NOLINTEND(readability-identifier-naming)

namespace igl::opengl::util::tests {

TEST(TextureFormatUtilTest, SizedColorFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA8), TextureFormat::RGBA_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_BGRA8_EXT), TextureFormat::BGRA_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB5_A1), TextureFormat::R5G5B5A1_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA4), TextureFormat::ABGR_UNorm4);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_ALPHA8), TextureFormat::A_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_LUMINANCE8), TextureFormat::L_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_LUMINANCE8_ALPHA8), TextureFormat::LA_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_LUMINANCE), TextureFormat::L_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_LUMINANCE_ALPHA), TextureFormat::LA_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB10_A2UI), TextureFormat::RGB10_A2_Uint_Rev);
}

TEST(TextureFormatUtilTest, SingleAndDualChannelSizedFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_R16), TextureFormat::R_UNorm16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_R16F), TextureFormat::R_F16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_R32F), TextureFormat::R_F32);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_R16UI), TextureFormat::R_UInt16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_R32UI), TextureFormat::R_UInt32);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RG16), TextureFormat::RG_UNorm16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RG16F), TextureFormat::RG_F16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RG16UI), TextureFormat::RG_UInt16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RG32F), TextureFormat::RG_F32);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA16), TextureFormat::RGBA_UNorm16);
}

TEST(TextureFormatUtilTest, FloatColorFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB16F), TextureFormat::RGB_F16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA16F), TextureFormat::RGBA_F16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB32F), TextureFormat::RGB_F32);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA32F), TextureFormat::RGBA_F32);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA32UI), TextureFormat::RGBA_UInt32);
}

TEST(TextureFormatUtilTest, SRGBFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_SRGB8_ALPHA8), TextureFormat::RGBA_SRGB);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_SRGB_ALPHA), TextureFormat::RGBA_SRGB);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_SRGB8_ALPHA8, GL_BGRA), TextureFormat::BGRA_SRGB);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_SRGB_ALPHA, GL_BGRA), TextureFormat::BGRA_SRGB);
}

TEST(TextureFormatUtilTest, RGB10A2DependsOnGlFormat) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB10_A2), TextureFormat::RGB10_A2_UNorm_Rev);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB10_A2, GL_BGRA), TextureFormat::BGR10_A2_Unorm);
}

TEST(TextureFormatUtilTest, ASTCCompressedFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_4x4_KHR),
            TextureFormat::RGBA_ASTC_4x4);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR),
            TextureFormat::SRGB8_A8_ASTC_4x4);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_5x4_KHR),
            TextureFormat::RGBA_ASTC_5x4);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR),
            TextureFormat::SRGB8_A8_ASTC_5x4);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_5x5_KHR),
            TextureFormat::RGBA_ASTC_5x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR),
            TextureFormat::SRGB8_A8_ASTC_5x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_6x5_KHR),
            TextureFormat::RGBA_ASTC_6x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR),
            TextureFormat::SRGB8_A8_ASTC_6x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_6x6_KHR),
            TextureFormat::RGBA_ASTC_6x6);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR),
            TextureFormat::SRGB8_A8_ASTC_6x6);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_8x5_KHR),
            TextureFormat::RGBA_ASTC_8x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR),
            TextureFormat::SRGB8_A8_ASTC_8x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_8x6_KHR),
            TextureFormat::RGBA_ASTC_8x6);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR),
            TextureFormat::SRGB8_A8_ASTC_8x6);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_8x8_KHR),
            TextureFormat::RGBA_ASTC_8x8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR),
            TextureFormat::SRGB8_A8_ASTC_8x8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x5_KHR),
            TextureFormat::RGBA_ASTC_10x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR),
            TextureFormat::SRGB8_A8_ASTC_10x5);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x6_KHR),
            TextureFormat::RGBA_ASTC_10x6);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR),
            TextureFormat::SRGB8_A8_ASTC_10x6);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x8_KHR),
            TextureFormat::RGBA_ASTC_10x8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR),
            TextureFormat::SRGB8_A8_ASTC_10x8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x10_KHR),
            TextureFormat::RGBA_ASTC_10x10);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR),
            TextureFormat::SRGB8_A8_ASTC_10x10);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_12x10_KHR),
            TextureFormat::RGBA_ASTC_12x10);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR),
            TextureFormat::SRGB8_A8_ASTC_12x10);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_ASTC_12x12_KHR),
            TextureFormat::RGBA_ASTC_12x12);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR),
            TextureFormat::SRGB8_A8_ASTC_12x12);
}

TEST(TextureFormatUtilTest, BPTCCompressedFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_BPTC_UNORM),
            TextureFormat::RGBA_BC7_UNORM_4x4);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM),
            TextureFormat::RGBA_BC7_SRGB_4x4);
}

TEST(TextureFormatUtilTest, PVRTCCompressedFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG),
            TextureFormat::RGBA_PVRTC_2BPPV1);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG),
            TextureFormat::RGB_PVRTC_2BPPV1);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG),
            TextureFormat::RGBA_PVRTC_4BPPV1);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG),
            TextureFormat::RGB_PVRTC_4BPPV1);
}

TEST(TextureFormatUtilTest, ETCAndEACCompressedFormats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_ETC1_RGB8_OES), TextureFormat::RGB8_ETC1);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGB8_ETC2), TextureFormat::RGB8_ETC2);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2),
            TextureFormat::RGB8_Punchthrough_A1_ETC2);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RGBA8_ETC2_EAC),
            TextureFormat::RGBA8_EAC_ETC2);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_R11_EAC), TextureFormat::R_EAC_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SIGNED_R11_EAC),
            TextureFormat::R_EAC_SNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_RG11_EAC), TextureFormat::RG_EAC_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SIGNED_RG11_EAC),
            TextureFormat::RG_EAC_SNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ETC2), TextureFormat::SRGB8_ETC2);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC),
            TextureFormat::SRGB8_A8_EAC_ETC2);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2),
            TextureFormat::SRGB8_Punchthrough_A1_ETC2);
}

TEST(TextureFormatUtilTest, UnsizedFormatsRequireGlFormatAndType) {
  // GL_RED + GL_RED + GL_UNSIGNED_BYTE -> R_UNorm8
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RED, GL_RED, GL_UNSIGNED_BYTE),
            TextureFormat::R_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RED, GL_RED, GL_UNSIGNED_SHORT),
            TextureFormat::Invalid);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RED), TextureFormat::Invalid);

  // GL_RG
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RG, GL_RG, GL_UNSIGNED_BYTE),
            TextureFormat::RG_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RG), TextureFormat::Invalid);

  // GL_RGB
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB, GL_RGB, GL_UNSIGNED_BYTE),
            TextureFormat::RGBX_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB), TextureFormat::Invalid);

  // GL_BGR
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_BGR, GL_BGR, GL_UNSIGNED_SHORT_5_6_5),
            TextureFormat::B5G6R5_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_BGR), TextureFormat::Invalid);
}

TEST(TextureFormatUtilTest, UnsizedRGBADisambiguation) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE),
            TextureFormat::RGBA_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1),
            TextureFormat::R5G5B5A1_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES),
            TextureFormat::RGBA_F16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV),
            TextureFormat::BGRA_UNorm8_Rev);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV),
            TextureFormat::RGB10_A2_UNorm_Rev);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA), TextureFormat::Invalid);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGBA, GL_RGB, GL_UNSIGNED_BYTE),
            TextureFormat::Invalid);
}

TEST(TextureFormatUtilTest, UnsizedBGRADisambiguation) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_BGRA, GL_BGRA, GL_UNSIGNED_BYTE),
            TextureFormat::BGRA_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_BGRA, GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1),
            TextureFormat::B5G5R5A1_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_BGRA), TextureFormat::Invalid);
}

TEST(TextureFormatUtilTest, UnsizedAlphaAndStencil) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE),
            TextureFormat::A_UNorm8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_ALPHA), TextureFormat::Invalid);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_STENCIL_INDEX, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE),
            TextureFormat::S_UInt8);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_STENCIL_INDEX), TextureFormat::Invalid);
}

TEST(TextureFormatUtilTest, AppleRGBRaw422Formats) {
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB_RAW_422_APPLE, 0, GL_UNSIGNED_SHORT_8_8_APPLE),
            TextureFormat::R4G2B2_UNorm_Apple);
  ASSERT_EQ(
      glTextureFormatToTextureFormat(GL_RGB_RAW_422_APPLE, 0, GL_UNSIGNED_SHORT_8_8_REV_APPLE),
      TextureFormat::R4G2B2_UNorm_Rev_Apple);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_RGB_RAW_422_APPLE), TextureFormat::Invalid);
}

TEST(TextureFormatUtilTest, DepthAndStencilFormats) {
  // Sized depth formats
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH_COMPONENT16), TextureFormat::Z_UNorm16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH_COMPONENT24), TextureFormat::Z_UNorm24);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH_COMPONENT32), TextureFormat::Z_UNorm32);

  // Sized stencil
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_STENCIL_INDEX8), TextureFormat::S_UInt8);

  // Combined depth/stencil sized
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH24_STENCIL8), TextureFormat::S8_UInt_Z24_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH32F_STENCIL8), TextureFormat::S8_UInt_Z32_UNorm);

  // Unsized GL_DEPTH_COMPONENT requires glFormat + glType
  ASSERT_EQ(
      glTextureFormatToTextureFormat(GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT),
      TextureFormat::Z_UNorm16);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT),
            TextureFormat::Z_UNorm32);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH_COMPONENT), TextureFormat::Invalid);

  // Unsized GL_DEPTH_STENCIL requires glFormat + glType
  ASSERT_EQ(
      glTextureFormatToTextureFormat(GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8),
      TextureFormat::S8_UInt_Z24_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(
                GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV),
            TextureFormat::S8_UInt_Z32_UNorm);
  ASSERT_EQ(glTextureFormatToTextureFormat(GL_DEPTH_STENCIL), TextureFormat::Invalid);
}

TEST(TextureFormatUtilTest, UnknownFormatsReturnInvalid) {
  ASSERT_EQ(glTextureFormatToTextureFormat(0), TextureFormat::Invalid);
  ASSERT_EQ(glTextureFormatToTextureFormat(0xDEAD), TextureFormat::Invalid);
  ASSERT_EQ(glTextureFormatToTextureFormat(0x1234, 0x5678, 0x9ABC), TextureFormat::Invalid);
}

} // namespace igl::opengl::util::tests
