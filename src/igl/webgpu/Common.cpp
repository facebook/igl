/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Common.h>

#include <atomic>
#include <igl/webgpu/WebGPUCompat.h>

namespace igl::webgpu {

namespace {

const char* getDefaultErrorMessage(WGPUErrorType type) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (type) {
  case WGPUErrorType_Validation:
    return "WebGPU validation error";
  case WGPUErrorType_OutOfMemory:
    return "WebGPU out of memory";
  case WGPUErrorType_Internal:
    return "WebGPU internal error";
  case WGPUErrorType_Unknown:
    return "WebGPU unknown error";
  default:
    return "WebGPU error";
  }
}

} // namespace

uint64_t allocateResourceId() {
  static std::atomic<uint64_t> counter = 0;
  return ++counter;
}

std::string toStdString(WGPUStringView view) {
  if (view.data == nullptr) {
    return {};
  }
  if (view.length == WGPU_STRLEN) {
    return {view.data};
  }
  return {view.data, view.length};
}

std::optional<WGPUTextureFormat> textureFormatToWGPUTextureFormat(TextureFormat format) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case TextureFormat::R_UNorm8:
    return WGPUTextureFormat_R8Unorm;
  case TextureFormat::R_F16:
    return WGPUTextureFormat_R16Float;
  case TextureFormat::R_UInt16:
    return WGPUTextureFormat_R16Uint;
  case TextureFormat::R_UNorm16:
    return WGPUTextureFormat_R16Unorm;
  case TextureFormat::RG_UNorm8:
    return WGPUTextureFormat_RG8Unorm;
  case TextureFormat::RGBA_UNorm8:
  case TextureFormat::RGBX_UNorm8:
    return WGPUTextureFormat_RGBA8Unorm;
  case TextureFormat::BGRA_UNorm8:
    return WGPUTextureFormat_BGRA8Unorm;
  case TextureFormat::RGBA_SRGB:
    return WGPUTextureFormat_RGBA8UnormSrgb;
  case TextureFormat::BGRA_SRGB:
    return WGPUTextureFormat_BGRA8UnormSrgb;
  case TextureFormat::RG_F16:
    return WGPUTextureFormat_RG16Float;
  case TextureFormat::RG_UInt16:
    return WGPUTextureFormat_RG16Uint;
  case TextureFormat::RG_UNorm16:
    return WGPUTextureFormat_RG16Unorm;
  case TextureFormat::RGB10_A2_UNorm_Rev:
    return WGPUTextureFormat_RGB10A2Unorm;
  case TextureFormat::RGB10_A2_Uint_Rev:
    return WGPUTextureFormat_RGB10A2Uint;
  case TextureFormat::B10G11R11_UFloat:
    return WGPUTextureFormat_RG11B10Ufloat;
  case TextureFormat::R_F32:
    return WGPUTextureFormat_R32Float;
  case TextureFormat::R_UInt32:
    return WGPUTextureFormat_R32Uint;
  case TextureFormat::RGBA_UNorm16:
    return WGPUTextureFormat_RGBA16Unorm;
  case TextureFormat::RGBA_F16:
    return WGPUTextureFormat_RGBA16Float;
  case TextureFormat::RG_F32:
    return WGPUTextureFormat_RG32Float;
  case TextureFormat::RGBA_UInt32:
    return WGPUTextureFormat_RGBA32Uint;
  case TextureFormat::RGBA_F32:
    return WGPUTextureFormat_RGBA32Float;
  case TextureFormat::RGBA_ASTC_4x4:
    return WGPUTextureFormat_ASTC4x4Unorm;
  case TextureFormat::SRGB8_A8_ASTC_4x4:
    return WGPUTextureFormat_ASTC4x4UnormSrgb;
  case TextureFormat::RGBA_ASTC_5x4:
    return WGPUTextureFormat_ASTC5x4Unorm;
  case TextureFormat::SRGB8_A8_ASTC_5x4:
    return WGPUTextureFormat_ASTC5x4UnormSrgb;
  case TextureFormat::RGBA_ASTC_5x5:
    return WGPUTextureFormat_ASTC5x5Unorm;
  case TextureFormat::SRGB8_A8_ASTC_5x5:
    return WGPUTextureFormat_ASTC5x5UnormSrgb;
  case TextureFormat::RGBA_ASTC_6x5:
    return WGPUTextureFormat_ASTC6x5Unorm;
  case TextureFormat::SRGB8_A8_ASTC_6x5:
    return WGPUTextureFormat_ASTC6x5UnormSrgb;
  case TextureFormat::RGBA_ASTC_6x6:
    return WGPUTextureFormat_ASTC6x6Unorm;
  case TextureFormat::SRGB8_A8_ASTC_6x6:
    return WGPUTextureFormat_ASTC6x6UnormSrgb;
  case TextureFormat::RGBA_ASTC_8x5:
    return WGPUTextureFormat_ASTC8x5Unorm;
  case TextureFormat::SRGB8_A8_ASTC_8x5:
    return WGPUTextureFormat_ASTC8x5UnormSrgb;
  case TextureFormat::RGBA_ASTC_8x6:
    return WGPUTextureFormat_ASTC8x6Unorm;
  case TextureFormat::SRGB8_A8_ASTC_8x6:
    return WGPUTextureFormat_ASTC8x6UnormSrgb;
  case TextureFormat::RGBA_ASTC_8x8:
    return WGPUTextureFormat_ASTC8x8Unorm;
  case TextureFormat::SRGB8_A8_ASTC_8x8:
    return WGPUTextureFormat_ASTC8x8UnormSrgb;
  case TextureFormat::RGBA_ASTC_10x5:
    return WGPUTextureFormat_ASTC10x5Unorm;
  case TextureFormat::SRGB8_A8_ASTC_10x5:
    return WGPUTextureFormat_ASTC10x5UnormSrgb;
  case TextureFormat::RGBA_ASTC_10x6:
    return WGPUTextureFormat_ASTC10x6Unorm;
  case TextureFormat::SRGB8_A8_ASTC_10x6:
    return WGPUTextureFormat_ASTC10x6UnormSrgb;
  case TextureFormat::RGBA_ASTC_10x8:
    return WGPUTextureFormat_ASTC10x8Unorm;
  case TextureFormat::SRGB8_A8_ASTC_10x8:
    return WGPUTextureFormat_ASTC10x8UnormSrgb;
  case TextureFormat::RGBA_ASTC_10x10:
    return WGPUTextureFormat_ASTC10x10Unorm;
  case TextureFormat::SRGB8_A8_ASTC_10x10:
    return WGPUTextureFormat_ASTC10x10UnormSrgb;
  case TextureFormat::RGBA_ASTC_12x10:
    return WGPUTextureFormat_ASTC12x10Unorm;
  case TextureFormat::SRGB8_A8_ASTC_12x10:
    return WGPUTextureFormat_ASTC12x10UnormSrgb;
  case TextureFormat::RGBA_ASTC_12x12:
    return WGPUTextureFormat_ASTC12x12Unorm;
  case TextureFormat::SRGB8_A8_ASTC_12x12:
    return WGPUTextureFormat_ASTC12x12UnormSrgb;
  // ETC1 data decodes identically as ETC2 RGB8.
  case TextureFormat::RGB8_ETC1:
  case TextureFormat::RGB8_ETC2:
    return WGPUTextureFormat_ETC2RGB8Unorm;
  case TextureFormat::SRGB8_ETC2:
    return WGPUTextureFormat_ETC2RGB8UnormSrgb;
  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
    return WGPUTextureFormat_ETC2RGB8A1Unorm;
  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
    return WGPUTextureFormat_ETC2RGB8A1UnormSrgb;
  case TextureFormat::RGBA8_EAC_ETC2:
    return WGPUTextureFormat_ETC2RGBA8Unorm;
  case TextureFormat::SRGB8_A8_EAC_ETC2:
    return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
  case TextureFormat::RG_EAC_UNorm:
    return WGPUTextureFormat_EACRG11Unorm;
  case TextureFormat::RG_EAC_SNorm:
    return WGPUTextureFormat_EACRG11Snorm;
  case TextureFormat::R_EAC_UNorm:
    return WGPUTextureFormat_EACR11Unorm;
  case TextureFormat::R_EAC_SNorm:
    return WGPUTextureFormat_EACR11Snorm;
  case TextureFormat::RGBA_BC7_UNORM_4x4:
    return WGPUTextureFormat_BC7RGBAUnorm;
  case TextureFormat::RGBA_BC7_SRGB_4x4:
    return WGPUTextureFormat_BC7RGBAUnormSrgb;
  case TextureFormat::Z_UNorm16:
    return WGPUTextureFormat_Depth16Unorm;
  // depth24plus has no defined byte layout: it cannot be copied to or from buffers.
  case TextureFormat::Z_UNorm24:
    return WGPUTextureFormat_Depth24Plus;
  case TextureFormat::Z_UNorm32:
    return WGPUTextureFormat_Depth32Float;
  case TextureFormat::S8_UInt_Z24_UNorm:
    return WGPUTextureFormat_Depth24PlusStencil8;
  case TextureFormat::S8_UInt_Z32_UNorm:
    return WGPUTextureFormat_Depth32FloatStencil8;
  case TextureFormat::S_UInt8:
    return WGPUTextureFormat_Stencil8;
  default:
    return std::nullopt;
  }
}

TextureFormat wgpuTextureFormatToTextureFormat(WGPUTextureFormat format) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case WGPUTextureFormat_R8Unorm:
    return TextureFormat::R_UNorm8;
  case WGPUTextureFormat_R16Float:
    return TextureFormat::R_F16;
  case WGPUTextureFormat_R16Uint:
    return TextureFormat::R_UInt16;
  case WGPUTextureFormat_R16Unorm:
    return TextureFormat::R_UNorm16;
  case WGPUTextureFormat_RG8Unorm:
    return TextureFormat::RG_UNorm8;
  case WGPUTextureFormat_RGBA8Unorm:
    return TextureFormat::RGBA_UNorm8;
  case WGPUTextureFormat_BGRA8Unorm:
    return TextureFormat::BGRA_UNorm8;
  case WGPUTextureFormat_RGBA8UnormSrgb:
    return TextureFormat::RGBA_SRGB;
  case WGPUTextureFormat_BGRA8UnormSrgb:
    return TextureFormat::BGRA_SRGB;
  case WGPUTextureFormat_RG16Float:
    return TextureFormat::RG_F16;
  case WGPUTextureFormat_RG16Uint:
    return TextureFormat::RG_UInt16;
  case WGPUTextureFormat_RG16Unorm:
    return TextureFormat::RG_UNorm16;
  case WGPUTextureFormat_RGB10A2Unorm:
    return TextureFormat::RGB10_A2_UNorm_Rev;
  case WGPUTextureFormat_RGB10A2Uint:
    return TextureFormat::RGB10_A2_Uint_Rev;
  case WGPUTextureFormat_RG11B10Ufloat:
    return TextureFormat::B10G11R11_UFloat;
  case WGPUTextureFormat_R32Float:
    return TextureFormat::R_F32;
  case WGPUTextureFormat_R32Uint:
    return TextureFormat::R_UInt32;
  case WGPUTextureFormat_RGBA16Unorm:
    return TextureFormat::RGBA_UNorm16;
  case WGPUTextureFormat_RGBA16Float:
    return TextureFormat::RGBA_F16;
  case WGPUTextureFormat_RG32Float:
    return TextureFormat::RG_F32;
  case WGPUTextureFormat_RGBA32Uint:
    return TextureFormat::RGBA_UInt32;
  case WGPUTextureFormat_RGBA32Float:
    return TextureFormat::RGBA_F32;
  case WGPUTextureFormat_ASTC4x4Unorm:
    return TextureFormat::RGBA_ASTC_4x4;
  case WGPUTextureFormat_ASTC4x4UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_4x4;
  case WGPUTextureFormat_ASTC5x4Unorm:
    return TextureFormat::RGBA_ASTC_5x4;
  case WGPUTextureFormat_ASTC5x4UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_5x4;
  case WGPUTextureFormat_ASTC5x5Unorm:
    return TextureFormat::RGBA_ASTC_5x5;
  case WGPUTextureFormat_ASTC5x5UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_5x5;
  case WGPUTextureFormat_ASTC6x5Unorm:
    return TextureFormat::RGBA_ASTC_6x5;
  case WGPUTextureFormat_ASTC6x5UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_6x5;
  case WGPUTextureFormat_ASTC6x6Unorm:
    return TextureFormat::RGBA_ASTC_6x6;
  case WGPUTextureFormat_ASTC6x6UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_6x6;
  case WGPUTextureFormat_ASTC8x5Unorm:
    return TextureFormat::RGBA_ASTC_8x5;
  case WGPUTextureFormat_ASTC8x5UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_8x5;
  case WGPUTextureFormat_ASTC8x6Unorm:
    return TextureFormat::RGBA_ASTC_8x6;
  case WGPUTextureFormat_ASTC8x6UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_8x6;
  case WGPUTextureFormat_ASTC8x8Unorm:
    return TextureFormat::RGBA_ASTC_8x8;
  case WGPUTextureFormat_ASTC8x8UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_8x8;
  case WGPUTextureFormat_ASTC10x5Unorm:
    return TextureFormat::RGBA_ASTC_10x5;
  case WGPUTextureFormat_ASTC10x5UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_10x5;
  case WGPUTextureFormat_ASTC10x6Unorm:
    return TextureFormat::RGBA_ASTC_10x6;
  case WGPUTextureFormat_ASTC10x6UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_10x6;
  case WGPUTextureFormat_ASTC10x8Unorm:
    return TextureFormat::RGBA_ASTC_10x8;
  case WGPUTextureFormat_ASTC10x8UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_10x8;
  case WGPUTextureFormat_ASTC10x10Unorm:
    return TextureFormat::RGBA_ASTC_10x10;
  case WGPUTextureFormat_ASTC10x10UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_10x10;
  case WGPUTextureFormat_ASTC12x10Unorm:
    return TextureFormat::RGBA_ASTC_12x10;
  case WGPUTextureFormat_ASTC12x10UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_12x10;
  case WGPUTextureFormat_ASTC12x12Unorm:
    return TextureFormat::RGBA_ASTC_12x12;
  case WGPUTextureFormat_ASTC12x12UnormSrgb:
    return TextureFormat::SRGB8_A8_ASTC_12x12;
  case WGPUTextureFormat_ETC2RGB8Unorm:
    return TextureFormat::RGB8_ETC2;
  case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    return TextureFormat::SRGB8_ETC2;
  case WGPUTextureFormat_ETC2RGB8A1Unorm:
    return TextureFormat::RGB8_Punchthrough_A1_ETC2;
  case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    return TextureFormat::SRGB8_Punchthrough_A1_ETC2;
  case WGPUTextureFormat_ETC2RGBA8Unorm:
    return TextureFormat::RGBA8_EAC_ETC2;
  case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    return TextureFormat::SRGB8_A8_EAC_ETC2;
  case WGPUTextureFormat_EACRG11Unorm:
    return TextureFormat::RG_EAC_UNorm;
  case WGPUTextureFormat_EACRG11Snorm:
    return TextureFormat::RG_EAC_SNorm;
  case WGPUTextureFormat_EACR11Unorm:
    return TextureFormat::R_EAC_UNorm;
  case WGPUTextureFormat_EACR11Snorm:
    return TextureFormat::R_EAC_SNorm;
  case WGPUTextureFormat_BC7RGBAUnorm:
    return TextureFormat::RGBA_BC7_UNORM_4x4;
  case WGPUTextureFormat_BC7RGBAUnormSrgb:
    return TextureFormat::RGBA_BC7_SRGB_4x4;
  case WGPUTextureFormat_Depth16Unorm:
    return TextureFormat::Z_UNorm16;
  case WGPUTextureFormat_Depth24Plus:
    return TextureFormat::Z_UNorm24;
  case WGPUTextureFormat_Depth32Float:
    return TextureFormat::Z_UNorm32;
  case WGPUTextureFormat_Depth24PlusStencil8:
    return TextureFormat::S8_UInt_Z24_UNorm;
  case WGPUTextureFormat_Depth32FloatStencil8:
    return TextureFormat::S8_UInt_Z32_UNorm;
  case WGPUTextureFormat_Stencil8:
    return TextureFormat::S_UInt8;
  default:
    return TextureFormat::Invalid;
  }
}

std::optional<WGPUFeatureName> getRequiredWGPUFeature(TextureFormat format) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case TextureFormat::R_UNorm16:
  case TextureFormat::RG_UNorm16:
  case TextureFormat::RGBA_UNorm16:
    return compat::kUnorm16TextureFormatsFeature;
  case TextureFormat::RGBA_ASTC_4x4:
  case TextureFormat::SRGB8_A8_ASTC_4x4:
  case TextureFormat::RGBA_ASTC_5x4:
  case TextureFormat::SRGB8_A8_ASTC_5x4:
  case TextureFormat::RGBA_ASTC_5x5:
  case TextureFormat::SRGB8_A8_ASTC_5x5:
  case TextureFormat::RGBA_ASTC_6x5:
  case TextureFormat::SRGB8_A8_ASTC_6x5:
  case TextureFormat::RGBA_ASTC_6x6:
  case TextureFormat::SRGB8_A8_ASTC_6x6:
  case TextureFormat::RGBA_ASTC_8x5:
  case TextureFormat::SRGB8_A8_ASTC_8x5:
  case TextureFormat::RGBA_ASTC_8x6:
  case TextureFormat::SRGB8_A8_ASTC_8x6:
  case TextureFormat::RGBA_ASTC_8x8:
  case TextureFormat::SRGB8_A8_ASTC_8x8:
  case TextureFormat::RGBA_ASTC_10x5:
  case TextureFormat::SRGB8_A8_ASTC_10x5:
  case TextureFormat::RGBA_ASTC_10x6:
  case TextureFormat::SRGB8_A8_ASTC_10x6:
  case TextureFormat::RGBA_ASTC_10x8:
  case TextureFormat::SRGB8_A8_ASTC_10x8:
  case TextureFormat::RGBA_ASTC_10x10:
  case TextureFormat::SRGB8_A8_ASTC_10x10:
  case TextureFormat::RGBA_ASTC_12x10:
  case TextureFormat::SRGB8_A8_ASTC_12x10:
  case TextureFormat::RGBA_ASTC_12x12:
  case TextureFormat::SRGB8_A8_ASTC_12x12:
    return WGPUFeatureName_TextureCompressionASTC;
  case TextureFormat::RGB8_ETC1:
  case TextureFormat::RGB8_ETC2:
  case TextureFormat::SRGB8_ETC2:
  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
  case TextureFormat::RGBA8_EAC_ETC2:
  case TextureFormat::SRGB8_A8_EAC_ETC2:
  case TextureFormat::RG_EAC_UNorm:
  case TextureFormat::RG_EAC_SNorm:
  case TextureFormat::R_EAC_UNorm:
  case TextureFormat::R_EAC_SNorm:
    return WGPUFeatureName_TextureCompressionETC2;
  case TextureFormat::RGBA_BC7_UNORM_4x4:
  case TextureFormat::RGBA_BC7_SRGB_4x4:
    return WGPUFeatureName_TextureCompressionBC;
  case TextureFormat::S8_UInt_Z32_UNorm:
    return WGPUFeatureName_Depth32FloatStencil8;
  default:
    return std::nullopt;
  }
}

std::optional<WGPUVertexFormat> vertexAttributeFormatToWGPUVertexFormat(
    VertexAttributeFormat format) {
  switch (format) {
  case VertexAttributeFormat::Float1:
    return WGPUVertexFormat_Float32;
  case VertexAttributeFormat::Float2:
    return WGPUVertexFormat_Float32x2;
  case VertexAttributeFormat::Float3:
    return WGPUVertexFormat_Float32x3;
  case VertexAttributeFormat::Float4:
    return WGPUVertexFormat_Float32x4;
  case VertexAttributeFormat::Byte1:
    return WGPUVertexFormat_Sint8;
  case VertexAttributeFormat::Byte2:
    return WGPUVertexFormat_Sint8x2;
  case VertexAttributeFormat::Byte4:
    return WGPUVertexFormat_Sint8x4;
  case VertexAttributeFormat::UByte1:
    return WGPUVertexFormat_Uint8;
  case VertexAttributeFormat::UByte2:
    return WGPUVertexFormat_Uint8x2;
  case VertexAttributeFormat::UByte4:
    return WGPUVertexFormat_Uint8x4;
  case VertexAttributeFormat::Short1:
    return WGPUVertexFormat_Sint16;
  case VertexAttributeFormat::Short2:
    return WGPUVertexFormat_Sint16x2;
  case VertexAttributeFormat::Short4:
    return WGPUVertexFormat_Sint16x4;
  case VertexAttributeFormat::UShort1:
    return WGPUVertexFormat_Uint16;
  case VertexAttributeFormat::UShort2:
    return WGPUVertexFormat_Uint16x2;
  case VertexAttributeFormat::UShort4:
    return WGPUVertexFormat_Uint16x4;
  case VertexAttributeFormat::Byte1Norm:
    return WGPUVertexFormat_Snorm8;
  case VertexAttributeFormat::Byte2Norm:
    return WGPUVertexFormat_Snorm8x2;
  case VertexAttributeFormat::Byte4Norm:
    return WGPUVertexFormat_Snorm8x4;
  case VertexAttributeFormat::UByte1Norm:
    return WGPUVertexFormat_Unorm8;
  case VertexAttributeFormat::UByte2Norm:
    return WGPUVertexFormat_Unorm8x2;
  case VertexAttributeFormat::UByte4Norm:
    return WGPUVertexFormat_Unorm8x4;
  case VertexAttributeFormat::Short1Norm:
    return WGPUVertexFormat_Snorm16;
  case VertexAttributeFormat::Short2Norm:
    return WGPUVertexFormat_Snorm16x2;
  case VertexAttributeFormat::Short4Norm:
    return WGPUVertexFormat_Snorm16x4;
  case VertexAttributeFormat::UShort1Norm:
    return WGPUVertexFormat_Unorm16;
  case VertexAttributeFormat::UShort2Norm:
    return WGPUVertexFormat_Unorm16x2;
  case VertexAttributeFormat::UShort4Norm:
    return WGPUVertexFormat_Unorm16x4;
  case VertexAttributeFormat::Int1:
    return WGPUVertexFormat_Sint32;
  case VertexAttributeFormat::Int2:
    return WGPUVertexFormat_Sint32x2;
  case VertexAttributeFormat::Int3:
    return WGPUVertexFormat_Sint32x3;
  case VertexAttributeFormat::Int4:
    return WGPUVertexFormat_Sint32x4;
  case VertexAttributeFormat::UInt1:
    return WGPUVertexFormat_Uint32;
  case VertexAttributeFormat::UInt2:
    return WGPUVertexFormat_Uint32x2;
  case VertexAttributeFormat::UInt3:
    return WGPUVertexFormat_Uint32x3;
  case VertexAttributeFormat::UInt4:
    return WGPUVertexFormat_Uint32x4;
  case VertexAttributeFormat::HalfFloat1:
    return WGPUVertexFormat_Float16;
  case VertexAttributeFormat::HalfFloat2:
    return WGPUVertexFormat_Float16x2;
  case VertexAttributeFormat::HalfFloat4:
    return WGPUVertexFormat_Float16x4;
  // WebGPU has no 3-component 8/16-bit formats and no signed 2_10_10_10 format.
  case VertexAttributeFormat::Byte3:
  case VertexAttributeFormat::UByte3:
  case VertexAttributeFormat::Short3:
  case VertexAttributeFormat::UShort3:
  case VertexAttributeFormat::Byte3Norm:
  case VertexAttributeFormat::UByte3Norm:
  case VertexAttributeFormat::Short3Norm:
  case VertexAttributeFormat::UShort3Norm:
  case VertexAttributeFormat::HalfFloat3:
  case VertexAttributeFormat::Int_2_10_10_10_REV:
    return std::nullopt;
  }
  IGL_UNREACHABLE_RETURN(std::nullopt)
}

std::optional<WGPUBlendFactor> blendFactorToWGPUBlendFactor(BlendFactor factor) {
  switch (factor) {
  case BlendFactor::Zero:
    return WGPUBlendFactor_Zero;
  case BlendFactor::One:
    return WGPUBlendFactor_One;
  case BlendFactor::SrcColor:
    return WGPUBlendFactor_Src;
  case BlendFactor::OneMinusSrcColor:
    return WGPUBlendFactor_OneMinusSrc;
  case BlendFactor::SrcAlpha:
    return WGPUBlendFactor_SrcAlpha;
  case BlendFactor::OneMinusSrcAlpha:
    return WGPUBlendFactor_OneMinusSrcAlpha;
  case BlendFactor::DstColor:
    return WGPUBlendFactor_Dst;
  case BlendFactor::OneMinusDstColor:
    return WGPUBlendFactor_OneMinusDst;
  case BlendFactor::DstAlpha:
    return WGPUBlendFactor_DstAlpha;
  case BlendFactor::OneMinusDstAlpha:
    return WGPUBlendFactor_OneMinusDstAlpha;
  case BlendFactor::SrcAlphaSaturated:
    return WGPUBlendFactor_SrcAlphaSaturated;
  case BlendFactor::BlendColor:
    return WGPUBlendFactor_Constant;
  case BlendFactor::OneMinusBlendColor:
    return WGPUBlendFactor_OneMinusConstant;
  case BlendFactor::Src1Color:
    return WGPUBlendFactor_Src1;
  case BlendFactor::OneMinusSrc1Color:
    return WGPUBlendFactor_OneMinusSrc1;
  case BlendFactor::Src1Alpha:
    return WGPUBlendFactor_Src1Alpha;
  case BlendFactor::OneMinusSrc1Alpha:
    return WGPUBlendFactor_OneMinusSrc1Alpha;
  // WebGPU has a single RGBA blend constant and no factor that broadcasts its alpha.
  case BlendFactor::BlendAlpha:
  case BlendFactor::OneMinusBlendAlpha:
    return std::nullopt;
  }
  IGL_UNREACHABLE_RETURN(std::nullopt)
}

WGPUBlendOperation blendOpToWGPUBlendOperation(BlendOp op) {
  switch (op) {
  case BlendOp::Add:
    return WGPUBlendOperation_Add;
  case BlendOp::Subtract:
    return WGPUBlendOperation_Subtract;
  case BlendOp::ReverseSubtract:
    return WGPUBlendOperation_ReverseSubtract;
  case BlendOp::Min:
    return WGPUBlendOperation_Min;
  case BlendOp::Max:
    return WGPUBlendOperation_Max;
  }
  IGL_UNREACHABLE_RETURN(WGPUBlendOperation_Undefined)
}

WGPUColorWriteMask colorWriteMaskToWGPUColorWriteMask(ColorWriteMask mask) {
  WGPUColorWriteMask result = WGPUColorWriteMask_None;
  if ((mask & kColorWriteBitsRed) != 0) {
    result |= WGPUColorWriteMask_Red;
  }
  if ((mask & kColorWriteBitsGreen) != 0) {
    result |= WGPUColorWriteMask_Green;
  }
  if ((mask & kColorWriteBitsBlue) != 0) {
    result |= WGPUColorWriteMask_Blue;
  }
  if ((mask & kColorWriteBitsAlpha) != 0) {
    result |= WGPUColorWriteMask_Alpha;
  }
  return result;
}

WGPUCompareFunction compareFunctionToWGPUCompareFunction(CompareFunction func) {
  switch (func) {
  case CompareFunction::Never:
    return WGPUCompareFunction_Never;
  case CompareFunction::Less:
    return WGPUCompareFunction_Less;
  case CompareFunction::Equal:
    return WGPUCompareFunction_Equal;
  case CompareFunction::LessEqual:
    return WGPUCompareFunction_LessEqual;
  case CompareFunction::Greater:
    return WGPUCompareFunction_Greater;
  case CompareFunction::NotEqual:
    return WGPUCompareFunction_NotEqual;
  case CompareFunction::GreaterEqual:
    return WGPUCompareFunction_GreaterEqual;
  case CompareFunction::AlwaysPass:
    return WGPUCompareFunction_Always;
  }
  IGL_UNREACHABLE_RETURN(WGPUCompareFunction_Undefined)
}

WGPUStencilOperation stencilOperationToWGPUStencilOperation(StencilOperation op) {
  switch (op) {
  case StencilOperation::Keep:
    return WGPUStencilOperation_Keep;
  case StencilOperation::Zero:
    return WGPUStencilOperation_Zero;
  case StencilOperation::Replace:
    return WGPUStencilOperation_Replace;
  case StencilOperation::IncrementClamp:
    return WGPUStencilOperation_IncrementClamp;
  case StencilOperation::DecrementClamp:
    return WGPUStencilOperation_DecrementClamp;
  case StencilOperation::Invert:
    return WGPUStencilOperation_Invert;
  case StencilOperation::IncrementWrap:
    return WGPUStencilOperation_IncrementWrap;
  case StencilOperation::DecrementWrap:
    return WGPUStencilOperation_DecrementWrap;
  }
  IGL_UNREACHABLE_RETURN(WGPUStencilOperation_Undefined)
}

std::optional<WGPUAddressMode> samplerAddressModeToWGPUAddressMode(SamplerAddressMode mode) {
  switch (mode) {
  case SamplerAddressMode::Repeat:
    return WGPUAddressMode_Repeat;
  case SamplerAddressMode::Clamp:
    return WGPUAddressMode_ClampToEdge;
  case SamplerAddressMode::MirrorRepeat:
    return WGPUAddressMode_MirrorRepeat;
  case SamplerAddressMode::ClampToBorder:
    return std::nullopt;
  }
  IGL_UNREACHABLE_RETURN(std::nullopt)
}

WGPUFilterMode samplerMinMagFilterToWGPUFilterMode(SamplerMinMagFilter filter) {
  switch (filter) {
  case SamplerMinMagFilter::Nearest:
    return WGPUFilterMode_Nearest;
  case SamplerMinMagFilter::Linear:
    return WGPUFilterMode_Linear;
  }
  IGL_UNREACHABLE_RETURN(WGPUFilterMode_Undefined)
}

WGPUMipmapFilterMode samplerMipFilterToWGPUMipmapFilterMode(SamplerMipFilter filter) {
  switch (filter) {
  case SamplerMipFilter::Disabled:
  case SamplerMipFilter::Nearest:
    return WGPUMipmapFilterMode_Nearest;
  case SamplerMipFilter::Linear:
    return WGPUMipmapFilterMode_Linear;
  }
  IGL_UNREACHABLE_RETURN(WGPUMipmapFilterMode_Undefined)
}

WGPUPrimitiveTopology primitiveTypeToWGPUPrimitiveTopology(PrimitiveType type) {
  switch (type) {
  case PrimitiveType::Point:
    return WGPUPrimitiveTopology_PointList;
  case PrimitiveType::Line:
    return WGPUPrimitiveTopology_LineList;
  case PrimitiveType::LineStrip:
    return WGPUPrimitiveTopology_LineStrip;
  case PrimitiveType::Triangle:
    return WGPUPrimitiveTopology_TriangleList;
  case PrimitiveType::TriangleStrip:
    return WGPUPrimitiveTopology_TriangleStrip;
  }
  IGL_UNREACHABLE_RETURN(WGPUPrimitiveTopology_Undefined)
}

WGPUCullMode cullModeToWGPUCullMode(CullMode mode) {
  switch (mode) {
  case CullMode::Disabled:
    return WGPUCullMode_None;
  case CullMode::Front:
    return WGPUCullMode_Front;
  case CullMode::Back:
    return WGPUCullMode_Back;
  }
  IGL_UNREACHABLE_RETURN(WGPUCullMode_Undefined)
}

WGPUFrontFace windingModeToWGPUFrontFace(WindingMode mode) {
  switch (mode) {
  case WindingMode::Clockwise:
    return WGPUFrontFace_CW;
  case WindingMode::CounterClockwise:
    return WGPUFrontFace_CCW;
  }
  IGL_UNREACHABLE_RETURN(WGPUFrontFace_Undefined)
}

std::optional<WGPUIndexFormat> indexFormatToWGPUIndexFormat(IndexFormat format) {
  switch (format) {
  case IndexFormat::UInt8:
    return std::nullopt;
  case IndexFormat::UInt16:
    return WGPUIndexFormat_Uint16;
  case IndexFormat::UInt32:
    return WGPUIndexFormat_Uint32;
  }
  IGL_UNREACHABLE_RETURN(std::nullopt)
}

std::optional<WGPUTextureDimension> textureTypeToWGPUTextureDimension(TextureType type) {
  switch (type) {
  case TextureType::TwoD:
  case TextureType::TwoDArray:
  case TextureType::Cube:
    return WGPUTextureDimension_2D;
  case TextureType::ThreeD:
    return WGPUTextureDimension_3D;
  case TextureType::Invalid:
  case TextureType::ExternalImage:
    return std::nullopt;
  }
  IGL_UNREACHABLE_RETURN(std::nullopt)
}

std::optional<WGPUTextureViewDimension> textureTypeToWGPUTextureViewDimension(TextureType type) {
  switch (type) {
  case TextureType::TwoD:
    return WGPUTextureViewDimension_2D;
  case TextureType::TwoDArray:
    return WGPUTextureViewDimension_2DArray;
  case TextureType::ThreeD:
    return WGPUTextureViewDimension_3D;
  case TextureType::Cube:
    return WGPUTextureViewDimension_Cube;
  case TextureType::Invalid:
  case TextureType::ExternalImage:
    return std::nullopt;
  }
  IGL_UNREACHABLE_RETURN(std::nullopt)
}

WGPULoadOp loadActionToWGPULoadOp(LoadAction action) {
  switch (action) {
  // WebGPU has no "don't care" load; clearing avoids reading the previous contents.
  case LoadAction::DontCare:
  case LoadAction::Clear:
    return WGPULoadOp_Clear;
  case LoadAction::Load:
    return WGPULoadOp_Load;
  }
  IGL_UNREACHABLE_RETURN(WGPULoadOp_Undefined)
}

WGPUStoreOp storeActionToWGPUStoreOp(StoreAction action) {
  switch (action) {
  case StoreAction::DontCare:
    return WGPUStoreOp_Discard;
  case StoreAction::Store:
    return WGPUStoreOp_Store;
  // The resolve target is always written; the multisampled attachment itself is not needed.
  case StoreAction::MsaaResolve:
    return WGPUStoreOp_Discard;
  }
  IGL_UNREACHABLE_RETURN(WGPUStoreOp_Undefined)
}

Result::Code wgpuErrorTypeToResultCode(WGPUErrorType type) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (type) {
  case WGPUErrorType_NoError:
    return Result::Code::Ok;
  case WGPUErrorType_Validation:
    return Result::Code::ArgumentInvalid;
  case WGPUErrorType_OutOfMemory:
    return Result::Code::ArgumentOutOfRange;
  case WGPUErrorType_Internal:
  case WGPUErrorType_Unknown:
  default:
    return Result::Code::RuntimeError;
  }
}

Result getResultFromWGPUError(WGPUErrorType type, WGPUStringView message) {
  const Result::Code code = wgpuErrorTypeToResultCode(type);
  if (code == Result::Code::Ok) {
    return Result();
  }
  std::string text = toStdString(message);
  if (text.empty()) {
    text = getDefaultErrorMessage(type);
  }
  return Result(code, std::move(text));
}

void setResultFrom(Result* IGL_NULLABLE outResult, WGPUErrorType type, WGPUStringView message) {
  if (!outResult) {
    return;
  }

  *outResult = getResultFromWGPUError(type, message);
}

} // namespace igl::webgpu
