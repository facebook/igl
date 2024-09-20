/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/TextureFormat.h>

#define IGL_COMMON_SKIP_CHECK
#include <igl/Assert.h>

namespace igl::vulkan::util {

using TextureFormat = ::igl::TextureFormat;

TextureFormat vkTextureFormatToTextureFormat(VkFormat vkFormat) {
  switch (vkFormat) {
  case VK_FORMAT_UNDEFINED:
    return TextureFormat::Invalid;
  case VK_FORMAT_R8_UNORM:
    return TextureFormat::R_UNorm8;
  case VK_FORMAT_R16_UNORM:
    return TextureFormat::R_UNorm16;
  case VK_FORMAT_R16_SFLOAT:
    return TextureFormat::R_F16;
  case VK_FORMAT_R16_UINT:
    return TextureFormat::R_UInt16;
  case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    return TextureFormat::B5G5R5A1_UNorm;
  case VK_FORMAT_B5G6R5_UNORM_PACK16:
    return TextureFormat::B5G6R5_UNorm;
  case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    return TextureFormat::ABGR_UNorm4;
  case VK_FORMAT_R8G8_UNORM:
    return TextureFormat::RG_UNorm8;
  case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    return TextureFormat::R5G5B5A1_UNorm;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return TextureFormat::BGRA_UNorm8;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return TextureFormat::RGBA_UNorm8;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return TextureFormat::RGBA_SRGB;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return TextureFormat::BGRA_SRGB;
  case VK_FORMAT_R16G16_UNORM:
    return TextureFormat::RG_UNorm16;
  case VK_FORMAT_R16G16_SFLOAT:
    return TextureFormat::RG_F16;
  case VK_FORMAT_R16G16_UINT:
    return TextureFormat::RG_UInt16;
  case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    return TextureFormat::RGB10_A2_UNorm_Rev;
  case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    return TextureFormat::RGB10_A2_Uint_Rev;
  case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    return TextureFormat::BGR10_A2_Unorm;
  case VK_FORMAT_R32_SFLOAT:
    return TextureFormat::R_F32;
  case VK_FORMAT_R32G32_SFLOAT:
    return TextureFormat::RG_F32;
  case VK_FORMAT_R16G16B16_SFLOAT:
    return TextureFormat::RGB_F16;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return TextureFormat::RGBA_F16;
  case VK_FORMAT_R32G32B32_SFLOAT:
    return TextureFormat::RGB_F32;
  case VK_FORMAT_R32G32B32A32_UINT:
    return TextureFormat::RGBA_UInt32;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return TextureFormat::RGBA_F32;
  case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_4x4;
  case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_4x4;
  case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_5x4;
  case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_5x4;
  case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_5x5;
  case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_5x5;
  case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_6x5;
  case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_6x5;
  case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_6x6;
  case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_6x6;
  case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_8x5;
  case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_8x5;
  case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_8x6;
  case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_8x6;
  case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_8x8;
  case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_8x8;
  case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_10x5;
  case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_10x5;
  case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_10x6;
  case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_10x6;
  case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_10x8;
  case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_10x8;
  case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_10x10;
  case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_10x10;
  case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_12x10;
  case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_12x10;
  case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    return TextureFormat::RGBA_ASTC_12x12;
  case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    return TextureFormat::SRGB8_A8_ASTC_12x12;
  case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    return TextureFormat::RGBA_PVRTC_2BPPV1;
  case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    return TextureFormat::RGBA_PVRTC_4BPPV1;
  case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    return TextureFormat::RGB8_ETC2;
  case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    return TextureFormat::SRGB8_ETC2;
  case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    return TextureFormat::RGB8_Punchthrough_A1_ETC2;
  case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    return TextureFormat::SRGB8_Punchthrough_A1_ETC2;
  case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    return TextureFormat::RG_EAC_UNorm;
  case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    return TextureFormat::RG_EAC_SNorm;
  case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    return TextureFormat::R_EAC_UNorm;
  case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    return TextureFormat::R_EAC_SNorm;
  case VK_FORMAT_D16_UNORM:
    return TextureFormat::Z_UNorm16;
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return TextureFormat::RGBA_BC7_UNORM_4x4;
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return TextureFormat::RGBA_BC7_SRGB_4x4;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return TextureFormat::Z_UNorm24;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return TextureFormat::S8_UInt_Z24_UNorm;
  case VK_FORMAT_S8_UINT:
    return TextureFormat::S_UInt8;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return TextureFormat::S8_UInt_Z32_UNorm;
  case VK_FORMAT_D32_SFLOAT:
    return TextureFormat::Z_UNorm32;
  default:
    IGL_ASSERT_MSG(false, "VkFormat value not handled: %d", (int)vkFormat);
  }

  return TextureFormat::Invalid;
}

VkFormat invertRedAndBlue(VkFormat format) {
  switch (format) {
  case VK_FORMAT_B8G8R8A8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  default:
    IGL_ASSERT_NOT_REACHED();
    return format;
  }
}

} // namespace igl::vulkan::util
