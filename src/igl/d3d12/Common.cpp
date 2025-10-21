/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

DXGI_FORMAT textureFormatToDXGIFormat(TextureFormat format) {
  switch (format) {
  case TextureFormat::Invalid:
    return DXGI_FORMAT_UNKNOWN;
  case TextureFormat::R_UNorm8:
    return DXGI_FORMAT_R8_UNORM;
  case TextureFormat::R_UNorm16:
    return DXGI_FORMAT_R16_UNORM;
  case TextureFormat::R_F16:
    return DXGI_FORMAT_R16_FLOAT;
  case TextureFormat::R_UInt16:
    return DXGI_FORMAT_R16_UINT;
  case TextureFormat::B5G5R5A1_UNorm:
    return DXGI_FORMAT_B5G5R5A1_UNORM;
  case TextureFormat::B5G6R5_UNorm:
    return DXGI_FORMAT_B5G6R5_UNORM;
  case TextureFormat::RG_UNorm8:
    return DXGI_FORMAT_R8G8_UNORM;
  case TextureFormat::RG_UNorm16:
    return DXGI_FORMAT_R16G16_UNORM;
  case TextureFormat::R5G5B5A1_UNorm:
    return DXGI_FORMAT_B5G5R5A1_UNORM; // DXGI closest match
  case TextureFormat::BGRA_UNorm8:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case TextureFormat::RGBA_UNorm8:
  case TextureFormat::RGBX_UNorm8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case TextureFormat::RGBA_SRGB:
    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  case TextureFormat::BGRA_SRGB:
    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  case TextureFormat::RG_F16:
    return DXGI_FORMAT_R16G16_FLOAT;
  case TextureFormat::RG_UInt16:
    return DXGI_FORMAT_R16G16_UINT;
  case TextureFormat::RGB10_A2_UNorm_Rev:
    return DXGI_FORMAT_R10G10B10A2_UNORM;
  case TextureFormat::RGB10_A2_Uint_Rev:
    return DXGI_FORMAT_R10G10B10A2_UINT;
  case TextureFormat::R_F32:
    return DXGI_FORMAT_R32_FLOAT;
  case TextureFormat::R_UInt32:
    return DXGI_FORMAT_R32_UINT;
  case TextureFormat::RG_F32:
    return DXGI_FORMAT_R32G32_FLOAT;
  case TextureFormat::RGB_F16:
    return DXGI_FORMAT_R16G16B16A16_FLOAT; // DXGI doesn't have RGB16, use RGBA16
  case TextureFormat::RGBA_F16:
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  case TextureFormat::RGB_F32:
    return DXGI_FORMAT_R32G32B32_FLOAT;
  case TextureFormat::RGBA_UInt32:
    return DXGI_FORMAT_R32G32B32A32_UINT;
  case TextureFormat::RGBA_F32:
    return DXGI_FORMAT_R32G32B32A32_FLOAT;
  // Depth/stencil formats
  case TextureFormat::Z_UNorm16:
    return DXGI_FORMAT_D16_UNORM;
  case TextureFormat::Z_UNorm24:
    return DXGI_FORMAT_D24_UNORM_S8_UINT; // DXGI doesn't have D24 alone
  case TextureFormat::Z_UNorm32:
    return DXGI_FORMAT_D32_FLOAT;
  case TextureFormat::S8_UInt_Z24_UNorm:
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
  case TextureFormat::S8_UInt_Z32_UNorm:
    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  case TextureFormat::S_UInt8:
    return DXGI_FORMAT_UNKNOWN; // DXGI doesn't support stencil-only
  default:
    return DXGI_FORMAT_UNKNOWN;
  }
}

TextureFormat dxgiFormatToTextureFormat(DXGI_FORMAT format) {
  switch (format) {
  case DXGI_FORMAT_UNKNOWN:
    return TextureFormat::Invalid;
  case DXGI_FORMAT_R8_UNORM:
    return TextureFormat::R_UNorm8;
  case DXGI_FORMAT_R16_UNORM:
    return TextureFormat::R_UNorm16;
  case DXGI_FORMAT_R16_FLOAT:
    return TextureFormat::R_F16;
  case DXGI_FORMAT_R8G8_UNORM:
    return TextureFormat::RG_UNorm8;
  case DXGI_FORMAT_R8G8B8A8_UNORM:
    return TextureFormat::RGBA_UNorm8;
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    return TextureFormat::RGBA_SRGB;
  case DXGI_FORMAT_B8G8R8A8_UNORM:
    return TextureFormat::BGRA_UNorm8;
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    return TextureFormat::BGRA_SRGB;
  case DXGI_FORMAT_R16G16B16A16_FLOAT:
    return TextureFormat::RGBA_F16;
  case DXGI_FORMAT_R32G32B32A32_FLOAT:
    return TextureFormat::RGBA_F32;
  case DXGI_FORMAT_D16_UNORM:
    return TextureFormat::Z_UNorm16;
  case DXGI_FORMAT_D24_UNORM_S8_UINT:
    return TextureFormat::S8_UInt_Z24_UNorm;
  case DXGI_FORMAT_D32_FLOAT:
    return TextureFormat::Z_UNorm32;
  default:
    return TextureFormat::Invalid;
  }
}

} // namespace igl::d3d12
