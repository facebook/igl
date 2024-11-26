/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/util/TextureFormat.h>

namespace igl::opengl::util {

// @fb-only

/// OpenGL Constants
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
#define GL_R32F 0x822E
#define GL_RED 0x1903
#define GL_RG 0x8227
#define GL_RG16 0x822C
#define GL_RG16F 0x822F
#define GL_RG16UI 0x823a
#define GL_RG32F 0x8230
#define GL_RGB 0x1907
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

TextureFormat glTextureFormatToTextureFormat(int32_t glInternalFormat,
                                             uint32_t glFormat,
                                             uint32_t glType) {
  switch (glInternalFormat) {
  case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    return TextureFormat::RGBA_ASTC_4x4;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    return TextureFormat::SRGB8_A8_ASTC_4x4;

  case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    return TextureFormat::RGBA_ASTC_5x4;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    return TextureFormat::SRGB8_A8_ASTC_5x4;

  case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    return TextureFormat::RGBA_ASTC_5x5;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    return TextureFormat::SRGB8_A8_ASTC_5x5;

  case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    return TextureFormat::RGBA_ASTC_6x5;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    return TextureFormat::SRGB8_A8_ASTC_6x5;

  case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    return TextureFormat::RGBA_ASTC_6x6;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    return TextureFormat::SRGB8_A8_ASTC_6x6;

  case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    return TextureFormat::RGBA_ASTC_8x5;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    return TextureFormat::SRGB8_A8_ASTC_8x5;

  case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    return TextureFormat::RGBA_ASTC_8x6;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    return TextureFormat::SRGB8_A8_ASTC_8x6;

  case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    return TextureFormat::RGBA_ASTC_8x8;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    return TextureFormat::SRGB8_A8_ASTC_8x8;

  case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    return TextureFormat::RGBA_ASTC_10x5;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    return TextureFormat::SRGB8_A8_ASTC_10x5;

  case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    return TextureFormat::RGBA_ASTC_10x6;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    return TextureFormat::SRGB8_A8_ASTC_10x6;

  case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    return TextureFormat::RGBA_ASTC_10x8;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    return TextureFormat::SRGB8_A8_ASTC_10x8;

  case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    return TextureFormat::RGBA_ASTC_10x10;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    return TextureFormat::SRGB8_A8_ASTC_10x10;

  case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    return TextureFormat::RGBA_ASTC_12x10;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    return TextureFormat::SRGB8_A8_ASTC_12x10;

  case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    return TextureFormat::RGBA_ASTC_12x12;

  case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    return TextureFormat::SRGB8_A8_ASTC_12x12;

  case GL_COMPRESSED_RGBA_BPTC_UNORM:
    return TextureFormat::RGBA_BC7_UNORM_4x4;

  case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
    return TextureFormat::RGBA_BC7_SRGB_4x4;

  case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
    return TextureFormat::RGBA_PVRTC_2BPPV1;

  case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    return TextureFormat::RGB_PVRTC_2BPPV1;

  case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    return TextureFormat::RGBA_PVRTC_4BPPV1;

  case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    return TextureFormat::RGB_PVRTC_4BPPV1;

  case GL_ETC1_RGB8_OES:
    return TextureFormat::RGB8_ETC1;

  case GL_COMPRESSED_RGB8_ETC2:
    return TextureFormat::RGB8_ETC2;

  case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    return TextureFormat::RGB8_Punchthrough_A1_ETC2;

  case GL_COMPRESSED_RGBA8_ETC2_EAC:
    return TextureFormat::RGBA8_EAC_ETC2;

  case GL_COMPRESSED_R11_EAC:
    return TextureFormat::R_EAC_UNorm;

  case GL_COMPRESSED_SIGNED_R11_EAC:
    return TextureFormat::R_EAC_SNorm;

  case GL_COMPRESSED_RG11_EAC:
    return TextureFormat::RG_EAC_UNorm;

  case GL_COMPRESSED_SIGNED_RG11_EAC:
    return TextureFormat::RG_EAC_SNorm;

  case GL_COMPRESSED_SRGB8_ETC2:
    return TextureFormat::SRGB8_ETC2;

  case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    return TextureFormat::SRGB8_A8_EAC_ETC2;

  case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    return TextureFormat::SRGB8_Punchthrough_A1_ETC2;

  case GL_RED:
    if (glFormat == GL_RED) {
      if (glType == GL_UNSIGNED_BYTE) {
        return TextureFormat::R_UNorm8;
      }
    }
    return TextureFormat::Invalid;

  case GL_RG:
    if (glFormat == GL_RG) {
      if (glType == GL_UNSIGNED_BYTE) {
        return TextureFormat::RG_UNorm8;
      }
    }
    return TextureFormat::Invalid;

  case GL_RGB:
    if (glFormat == GL_RGB) {
      if (glType == GL_UNSIGNED_BYTE) {
        return TextureFormat::RGBX_UNorm8;
      }
    }
    return TextureFormat::Invalid;

  case GL_BGR:
    if (glFormat == GL_BGR) {
      if (glType == GL_UNSIGNED_SHORT_5_6_5) {
        return TextureFormat::B5G6R5_UNorm;
      }
    }
    return TextureFormat::Invalid;

  case GL_RGBA:
    if (glFormat == GL_RGBA) {
      if (glType == GL_UNSIGNED_BYTE) {
        return TextureFormat::RGBA_UNorm8;
      } else if (glType == GL_UNSIGNED_SHORT_5_5_5_1) {
        return TextureFormat::R5G5B5A1_UNorm;
      } else if (glType == GL_HALF_FLOAT_OES) {
        return TextureFormat::RGBA_F16;
      } else if (glType == GL_UNSIGNED_INT_8_8_8_8_REV) {
        return TextureFormat::BGRA_UNorm8_Rev;
      } else if (glType == GL_UNSIGNED_INT_2_10_10_10_REV) {
        return TextureFormat::RGB10_A2_UNorm_Rev;
      }
    }
    return TextureFormat::Invalid;

  case GL_RGBA8:
    return TextureFormat::RGBA_UNorm8;

  case GL_SRGB_ALPHA:
  case GL_SRGB8_ALPHA8:
    if (glFormat == GL_BGRA) {
      return TextureFormat::BGRA_SRGB;
    } else {
      return TextureFormat::RGBA_SRGB;
    }

  case GL_RGB5_A1:
    return TextureFormat::R5G5B5A1_UNorm;

  case GL_RGB10_A2UI:
    return TextureFormat::RGB10_A2_Uint_Rev;

  case GL_RGB10_A2:
    if (glFormat == GL_BGRA) {
      return TextureFormat::BGR10_A2_Unorm;
    } else {
      return TextureFormat::RGB10_A2_UNorm_Rev;
    }

  case GL_LUMINANCE_ALPHA:
  case GL_LUMINANCE8_ALPHA8:
    return TextureFormat::LA_UNorm8;

  case GL_LUMINANCE8:
  case GL_LUMINANCE:
    return TextureFormat::L_UNorm8;

  case GL_BGRA8_EXT:
    return TextureFormat::BGRA_UNorm8;

  case GL_BGRA:
    if (glFormat == GL_BGRA) {
      if (glType == GL_UNSIGNED_BYTE) {
        return TextureFormat::BGRA_UNorm8;
      } else if (glType == GL_UNSIGNED_SHORT_5_5_5_1) {
        return TextureFormat::B5G5R5A1_UNorm;
      }
    }
    return TextureFormat::Invalid;

  case GL_RGBA4:
    return TextureFormat::ABGR_UNorm4;

  case GL_ALPHA8:
    return TextureFormat::A_UNorm8;

  case GL_ALPHA:
    if (glFormat == GL_ALPHA) {
      if (glType == GL_UNSIGNED_BYTE) {
        // When GL_ALPHA is not supported (eg. desktop gl3) and A_UNorm8 is returned from this
        // method, an igl texture will get created with GL_Red and swizzled to the alpha instead of
        // GL_ALPHA
        return TextureFormat::A_UNorm8;
      }
    }
    return TextureFormat::Invalid;

  case GL_R16:
    return TextureFormat::R_UNorm16;

  case GL_R16F:
    return TextureFormat::R_F16;

  case GL_R32F:
    return TextureFormat::R_F32;

  case GL_R16UI:
    return TextureFormat::R_UInt16;

  case GL_RG16:
    return TextureFormat::RG_UNorm16;

  case GL_RG16F:
    return TextureFormat::RG_F16;

  case GL_RG16UI:
    return TextureFormat::RG_UInt16;

  case GL_RG32F:
    return TextureFormat::RG_F32;

  case GL_RGB_RAW_422_APPLE:
    if (glType == GL_UNSIGNED_SHORT_8_8_APPLE) {
      return TextureFormat::R4G2B2_UNorm_Apple;
    } else if (glType == GL_UNSIGNED_SHORT_8_8_REV_APPLE) {
      return TextureFormat::R4G2B2_UNorm_Rev_Apple;
    }
    return TextureFormat::Invalid;

  case GL_RGB16F:
    return TextureFormat::RGB_F16;

  case GL_RGBA16F:
    return TextureFormat::RGBA_F16;

  case GL_RGB32F:
    return TextureFormat::RGB_F32;

  case GL_RGBA32F:
    return TextureFormat::RGBA_F32;

  case GL_RGBA32UI:
    return TextureFormat::RGBA_UInt32;

  case GL_DEPTH_COMPONENT:
    if (glFormat == GL_DEPTH_COMPONENT) {
      if (glType == GL_UNSIGNED_SHORT) {
        return TextureFormat::Z_UNorm16;
      } else if (glType == GL_UNSIGNED_INT) {
        return TextureFormat::Z_UNorm32;
      }
    }
    return TextureFormat::Invalid;

  case GL_DEPTH_COMPONENT16:
    return TextureFormat::Z_UNorm16;

  case GL_DEPTH_COMPONENT24:
    return TextureFormat::Z_UNorm24;

  case GL_DEPTH_COMPONENT32:
    return TextureFormat::Z_UNorm32;

  case GL_DEPTH_STENCIL:
    if (glFormat == GL_DEPTH_STENCIL) {
      if (glType == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
        return TextureFormat::S8_UInt_Z32_UNorm;
      } else if (glType == GL_UNSIGNED_INT_24_8) {
        return TextureFormat::S8_UInt_Z24_UNorm;
      }
    }
    return TextureFormat::Invalid;

  case GL_DEPTH32F_STENCIL8:
    // TODO: Fix this once we properly support 32F depth formats
    return TextureFormat::S8_UInt_Z32_UNorm;

  case GL_DEPTH24_STENCIL8:
    return TextureFormat::S8_UInt_Z24_UNorm;

  case GL_STENCIL_INDEX8:
    return TextureFormat::S_UInt8;

  case GL_STENCIL_INDEX:
    if (glFormat == GL_STENCIL_INDEX) {
      if (glType == GL_UNSIGNED_BYTE) {
        return TextureFormat::S_UInt8;
      }
    }
    return TextureFormat::Invalid;

  default:
    return TextureFormat::Invalid;
  }

  return TextureFormat::Invalid;
}

} // namespace igl::opengl::util
