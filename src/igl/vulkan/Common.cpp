/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Common.h"

#include <array>
#include <cstdlib>

// clang-format off
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
// clang-format on

#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanShaderModule.h>
#include <igl/vulkan/VulkanTexture.h>
#include <igl/vulkan/util/SpvReflection.h>
#include <igl/vulkan/util/TextureFormat.h>

namespace igl::vulkan {

Result getResultFromVkResult(VkResult result) {
  if (result == VK_SUCCESS) {
    return Result();
  }

  Result res(Result::Code::RuntimeError, ivkGetVulkanResultString(result));

  switch (result) {
  case VK_ERROR_LAYER_NOT_PRESENT:
  case VK_ERROR_EXTENSION_NOT_PRESENT:
  case VK_ERROR_FEATURE_NOT_PRESENT:
    res.code = Result::Code::Unimplemented;
    return res;
  case VK_ERROR_INCOMPATIBLE_DRIVER:
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    res.code = Result::Code::Unsupported;
    return res;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
  case VK_ERROR_OUT_OF_POOL_MEMORY:
  case VK_ERROR_TOO_MANY_OBJECTS:
    res.code = Result::Code::ArgumentOutOfRange;
    return res;
  default:;
    // skip other Vulkan error codes
  }
  return res;
}

void setResultFrom(Result* outResult, VkResult result) {
  if (!outResult) {
    return;
  }

  *outResult = getResultFromVkResult(result);
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
    IGL_UNREACHABLE_RETURN(format);
  }
}

VkStencilOp stencilOperationToVkStencilOp(StencilOperation op) {
  switch (op) {
  case igl::StencilOperation::Keep:
    return VK_STENCIL_OP_KEEP;
  case igl::StencilOperation::Zero:
    return VK_STENCIL_OP_ZERO;
  case igl::StencilOperation::Replace:
    return VK_STENCIL_OP_REPLACE;
  case igl::StencilOperation::IncrementClamp:
    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case igl::StencilOperation::DecrementClamp:
    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case igl::StencilOperation::Invert:
    return VK_STENCIL_OP_INVERT;
  case igl::StencilOperation::IncrementWrap:
    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case igl::StencilOperation::DecrementWrap:
    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  IGL_DEBUG_ASSERT_NOT_REACHED();
  return VK_STENCIL_OP_KEEP;
}

VkFormat textureFormatToVkFormat(TextureFormat format) {
  using TextureFormat = TextureFormat;
  switch (format) {
  case TextureFormat::Invalid:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::A_UNorm8:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::L_UNorm8:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::R_UNorm8:
    return VK_FORMAT_R8_UNORM;
  case TextureFormat::R_UNorm16:
    return VK_FORMAT_R16_UNORM;
  case TextureFormat::R_F16:
    return VK_FORMAT_R16_SFLOAT;
  case TextureFormat::R_UInt16:
    return VK_FORMAT_R16_UINT;
  case TextureFormat::B5G5R5A1_UNorm:
    return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
  case TextureFormat::B5G6R5_UNorm:
    return VK_FORMAT_B5G6R5_UNORM_PACK16;
  case TextureFormat::ABGR_UNorm4:
    return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
  case TextureFormat::LA_UNorm8:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::RG_UNorm8:
    return VK_FORMAT_R8G8_UNORM;
  case TextureFormat::RG_UNorm16:
    return VK_FORMAT_R16G16_UNORM;
  case TextureFormat::R4G2B2_UNorm_Apple:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::R4G2B2_UNorm_Rev_Apple:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::R5G5B5A1_UNorm:
    return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
  case TextureFormat::BGRA_UNorm8:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case TextureFormat::BGRA_UNorm8_Rev:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::RGBA_UNorm8:
  case TextureFormat::RGBX_UNorm8:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case TextureFormat::RGBA_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case TextureFormat::BGRA_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case TextureFormat::RG_F16:
    return VK_FORMAT_R16G16_SFLOAT;
  case TextureFormat::RG_UInt16:
    return VK_FORMAT_R16G16_UINT;
  case TextureFormat::RGB10_A2_UNorm_Rev:
    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case TextureFormat::RGB10_A2_Uint_Rev:
    return VK_FORMAT_A2R10G10B10_UINT_PACK32;
  case TextureFormat::BGR10_A2_Unorm:
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  case TextureFormat::R_F32:
    return VK_FORMAT_R32_SFLOAT;
  case TextureFormat::R_UInt32:
    return VK_FORMAT_R32_UINT;
  case TextureFormat::RG_F32:
    return VK_FORMAT_R32G32_SFLOAT;
  case TextureFormat::RGB_F16:
    return VK_FORMAT_R16G16B16_SFLOAT;
  case TextureFormat::RGBA_F16:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case TextureFormat::RGB_F32:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case TextureFormat::RGBA_UInt32:
    return VK_FORMAT_R32G32B32A32_UINT;
  case TextureFormat::RGBA_F32:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case TextureFormat::RGBA_ASTC_4x4:
    return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_4x4:
    return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_5x4:
    return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_5x4:
    return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_5x5:
    return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_5x5:
    return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_6x5:
    return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_6x5:
    return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_6x6:
    return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_6x6:
    return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_8x5:
    return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_8x5:
    return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_8x6:
    return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_8x6:
    return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_8x8:
    return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_8x8:
    return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_10x5:
    return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_10x5:
    return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_10x6:
    return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_10x6:
    return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_10x8:
    return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_10x8:
    return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_10x10:
    return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_10x10:
    return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_12x10:
    return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_12x10:
    return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
  case TextureFormat::RGBA_ASTC_12x12:
    return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
  case TextureFormat::SRGB8_A8_ASTC_12x12:
    return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
  case TextureFormat::RGBA_PVRTC_2BPPV1:
    return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
  case TextureFormat::RGB_PVRTC_2BPPV1:
    return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
  case TextureFormat::RGBA_PVRTC_4BPPV1:
    return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
  case TextureFormat::RGB_PVRTC_4BPPV1:
    return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
  case TextureFormat::RGB8_ETC1:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::RGB8_ETC2:
    return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
  case TextureFormat::SRGB8_ETC2:
    return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
    return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
    return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
  case TextureFormat::RGBA8_EAC_ETC2:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::SRGB8_A8_EAC_ETC2:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::RG_EAC_UNorm:
    return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
  case TextureFormat::RG_EAC_SNorm:
    return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
  case TextureFormat::R_EAC_UNorm:
    return VK_FORMAT_EAC_R11_UNORM_BLOCK;
  case TextureFormat::R_EAC_SNorm:
    return VK_FORMAT_EAC_R11_SNORM_BLOCK;
  case TextureFormat::RGBA_BC7_UNORM_4x4:
    return VK_FORMAT_BC7_UNORM_BLOCK;
  case TextureFormat::RGBA_BC7_SRGB_4x4:
    return VK_FORMAT_BC7_SRGB_BLOCK;
  case TextureFormat::Z_UNorm16:
    return VK_FORMAT_D16_UNORM;
  case TextureFormat::Z_UNorm24:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case TextureFormat::Z_UNorm32:
    return VK_FORMAT_D32_SFLOAT;
  case TextureFormat::S8_UInt_Z24_UNorm:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case TextureFormat::S8_UInt_Z32_UNorm:
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  case TextureFormat::S_UInt8:
    return VK_FORMAT_S8_UINT;
  case TextureFormat::YUV_NV12:
    return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
  case TextureFormat::YUV_420p:
    return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
  // @fb-only
  // @fb-only
  // @fb-only
  // @fb-only
  // @fb-only
    // @fb-only
  }
  IGL_UNREACHABLE_RETURN(VK_FORMAT_UNDEFINED)
}

bool isTextureFormatRGB(VkFormat format) {
  return format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB ||
         format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
}

bool isTextureFormatBGR(VkFormat format) {
  return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB ||
         format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
}

TextureFormat vkFormatToTextureFormat(VkFormat format) {
  return util::vkTextureFormatToTextureFormat(static_cast<int32_t>(format));
}

VkMemoryPropertyFlags resourceStorageToVkMemoryPropertyFlags(
    ResourceStorage resourceStorage,
    const VkPhysicalDeviceMemoryProperties* IGL_NULLABLE memProperties) {
  VkMemoryPropertyFlags memFlags{0};

  switch (resourceStorage) {
  case ResourceStorage::Invalid:
    IGL_DEBUG_ABORT("Invalid storage type");
    break;
  case ResourceStorage::Private:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  case ResourceStorage::Shared:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case ResourceStorage::Managed:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case ResourceStorage::Memoryless: {
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (!memProperties) {
      break;
    }
    constexpr VkMemoryPropertyFlags targetFlags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    const auto* const memoryTypesEnd = memProperties->memoryTypes + memProperties->memoryTypeCount;
    if (std::find_if(
            memProperties->memoryTypes, memoryTypesEnd, [](const VkMemoryType& memoryType) {
              return (memoryType.propertyFlags & targetFlags) == targetFlags;
            }) != memoryTypesEnd) {
      memFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    break;
  }
  }

  return memFlags;
}

VkCompareOp compareFunctionToVkCompareOp(CompareFunction func) {
  switch (func) {
  case igl::CompareFunction::Never:
    return VK_COMPARE_OP_NEVER;
  case igl::CompareFunction::Less:
    return VK_COMPARE_OP_LESS;
  case igl::CompareFunction::Equal:
    return VK_COMPARE_OP_EQUAL;
  case igl::CompareFunction::LessEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case igl::CompareFunction::Greater:
    return VK_COMPARE_OP_GREATER;
  case igl::CompareFunction::NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case igl::CompareFunction::GreaterEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case igl::CompareFunction::AlwaysPass:
    return VK_COMPARE_OP_ALWAYS;
  }
  IGL_DEBUG_ABORT("CompareFunction value not handled: %d", (int)func);
  return VK_COMPARE_OP_ALWAYS;
}

VkColorSpaceKHR colorSpaceToVkColorSpace(ColorSpace colorSpace) {
  switch (colorSpace) {
  case ColorSpace::SRGB_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT; // closest thing to linear srgb
  case ColorSpace::SRGB_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  case ColorSpace::DISPLAY_P3_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
  case ColorSpace::DISPLAY_P3_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
  case ColorSpace::EXTENDED_SRGB_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
  case ColorSpace::DCI_P3_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
  case ColorSpace::BT709_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT;
  case ColorSpace::BT709_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
  case ColorSpace::BT2020_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT2020_LINEAR_EXT;
  case ColorSpace::HDR10_ST2084:
    return VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_ST2084_EXT;
  case ColorSpace::DOLBYVISION:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DOLBYVISION_EXT;
  case ColorSpace::HDR10_HLG:
    return VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_HLG_EXT;
  case ColorSpace::ADOBERGB_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT;
  case ColorSpace::ADOBERGB_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
  case ColorSpace::PASS_THROUGH:
    return VkColorSpaceKHR::VK_COLOR_SPACE_PASS_THROUGH_EXT;
  case ColorSpace::EXTENDED_SRGB_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
  case ColorSpace::DISPLAY_NATIVE_AMD:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_NATIVE_AMD;
  case ColorSpace::BT2020_NONLINEAR:
  case ColorSpace::BT601_NONLINEAR:
  case ColorSpace::BT2100_HLG_NONLINEAR:
  case ColorSpace::BT2100_PQ_NONLINEAR:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
  }
  IGL_UNREACHABLE_RETURN(VK_COLOR_SPACE_BT709_NONLINEAR_EXT);
}

ColorSpace vkColorSpaceToColorSpace(VkColorSpaceKHR colorSpace) {
  switch (colorSpace) {
  case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
    return ColorSpace::SRGB_NONLINEAR;
  case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
    return ColorSpace::DISPLAY_P3_NONLINEAR;
  case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
    return ColorSpace::EXTENDED_SRGB_LINEAR;
  case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
    return ColorSpace::DISPLAY_P3_LINEAR;
  case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:
    return ColorSpace::DCI_P3_NONLINEAR;
  case VK_COLOR_SPACE_BT709_LINEAR_EXT:
    return ColorSpace::BT709_LINEAR;
  case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
    return ColorSpace::BT709_NONLINEAR;
  case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
    return ColorSpace::BT2020_LINEAR;
  case VK_COLOR_SPACE_HDR10_ST2084_EXT:
    return ColorSpace::HDR10_ST2084;
  case VK_COLOR_SPACE_DOLBYVISION_EXT:
    return ColorSpace::DOLBYVISION;
  case VK_COLOR_SPACE_HDR10_HLG_EXT:
    return ColorSpace::HDR10_HLG;
  case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
    return ColorSpace::ADOBERGB_LINEAR;
  case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
    return ColorSpace::ADOBERGB_NONLINEAR;
  case VK_COLOR_SPACE_PASS_THROUGH_EXT:
    return ColorSpace::PASS_THROUGH;
  case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
    return ColorSpace::EXTENDED_SRGB_NONLINEAR;
  case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:
    return ColorSpace::DISPLAY_NATIVE_AMD;
  default:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return ColorSpace::SRGB_NONLINEAR;
  }
}

VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples) {
  if (numSamples <= 1) {
    return VK_SAMPLE_COUNT_1_BIT;
  }
  if (numSamples <= 2) {
    return VK_SAMPLE_COUNT_2_BIT;
  }
  if (numSamples <= 4) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if (numSamples <= 8) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if (numSamples <= 16) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if (numSamples <= 32) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  return VK_SAMPLE_COUNT_64_BIT;
}

uint32_t getVkLayer(TextureType type, uint32_t face, uint32_t layer) {
  return type == TextureType::Cube ? face : layer;
}

TextureRangeDesc atVkLayer(TextureType type, const TextureRangeDesc& range, uint32_t vkLayer) {
  return type == TextureType::Cube ? range.atFace(vkLayer) : range.atLayer(vkLayer);
}

void transitionToGeneral(VkCommandBuffer cmdBuf, ITexture* texture) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!texture) {
    return;
  }

  const vulkan::Texture& tex = static_cast<Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image_;
  const vulkan::VulkanImageView& imgView = tex.getVulkanTexture().imageView_;

  if (!img.isStorageImage()) {
    IGL_DEBUG_ABORT("Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader, otherwise wait for previous attachment writes
  const VkPipelineStageFlags srcStage =
      (img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
      : img.isDepthOrStencilFormat_                 ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                                    : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  img.transitionLayout(cmdBuf,
                       VK_IMAGE_LAYOUT_GENERAL,
                       srcStage,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VkImageSubresourceRange{imgView.getVkImageAspectFlags(),
                                               0,
                                               VK_REMAINING_MIP_LEVELS,
                                               0,
                                               VK_REMAINING_ARRAY_LAYERS});
}

void transitionToColorAttachment(VkCommandBuffer cmdBuf, ITexture* colorTex) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!colorTex) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*colorTex);
  const igl::vulkan::VulkanImage& img = vkTex.getVulkanTexture().image_;
  if (IGL_DEBUG_VERIFY_NOT(img.isDepthFormat_ || img.isStencilFormat_)) {
    IGL_DEBUG_ABORT("Color attachments cannot have depth/stencil formats");
    IGL_LOG_ERROR("Color attachments cannot have depth/stencil formats");
    return;
  }
  IGL_DEBUG_ASSERT(img.imageFormat_ != VK_FORMAT_UNDEFINED || vkTex.getVkExtendedFormat() != 0,
                   "Invalid color attachment format");
  if (!IGL_DEBUG_VERIFY((img.usageFlags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)) {
    IGL_DEBUG_ABORT("Did you forget to specify TextureUsageBit::Attachment usage bit?");
    IGL_LOG_ERROR("Did you forget to specify TextureUsageBit::Attachment usage bit?");
  }
  if (img.usageFlags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    // transition to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    img.transitionLayout(
        cmdBuf,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent fragment/compute
                                                  // shaders
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void transitionToDepthStencilAttachment(VkCommandBuffer cmdBuf, ITexture* depthStencilTex) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!depthStencilTex) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*depthStencilTex);
  const igl::vulkan::VulkanImage& img = vkTex.getVulkanTexture().image_;
  if (IGL_DEBUG_VERIFY_NOT(!img.isDepthFormat_ && !img.isStencilFormat_)) {
    IGL_DEBUG_ABORT("Only depth/stencil formats are accepted");
    IGL_LOG_ERROR("Only depth/stencil formats are accepted");
    return;
  }
  IGL_DEBUG_ASSERT(img.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!IGL_DEBUG_VERIFY((img.usageFlags_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)) {
    IGL_DEBUG_ABORT("Did you forget to specify TextureUsageBit::Attachment usage bit?");
    IGL_LOG_ERROR("Did you forget to specify TextureUsageBit::Attachment usage bit?");
  }
  if (img.usageFlags_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    // transition to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    VkImageAspectFlags aspectFlags = 0;
    if (img.isDepthFormat_) {
      aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (img.isStencilFormat_) {
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    img.transitionLayout(
        cmdBuf,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent fragment/compute
                                                  // shaders
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VkImageSubresourceRange{
            aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void transitionToShaderReadOnly(VkCommandBuffer cmdBuf, ITexture* texture) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  if (!texture) {
    return;
  }

  const vulkan::Texture& tex = static_cast<Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image_;
  const igl::vulkan::VulkanImageView& imgView = tex.getVulkanTexture().imageView_;

  const bool isColor = (imgView.getVkImageAspectFlags() & VK_IMAGE_ASPECT_COLOR_BIT) > 0;

  if (img.usageFlags_ & VK_IMAGE_USAGE_SAMPLED_BIT) {
    // transition sampled images to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    img.transitionLayout(
        cmdBuf,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        isColor ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for subsequent
                                                  // fragment/compute shaders
        VkImageSubresourceRange{
            img.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void overrideImageLayout(ITexture* texture, VkImageLayout layout) {
  if (!texture) {
    return;
  }
  const vulkan::Texture* tex = static_cast<Texture*>(texture);
  tex->getVulkanTexture().image_.imageLayout_ = layout;
}

void ensureShaderModule(IShaderModule* sm) {
  IGL_DEBUG_ASSERT(sm);

  const igl::vulkan::util::SpvModuleInfo& info =
      static_cast<ShaderModule*>(sm)->getVulkanShaderModule().getSpvModuleInfo();

  for (const auto& t : info.textures) {
    if (!IGL_DEBUG_VERIFY(t.descriptorSet == kBindPoint_CombinedImageSamplers)) {
      IGL_LOG_ERROR(
          "Missing descriptor set id for textures: the shader should contain \"layout(set = "
          "%u, ...)\"",
          kBindPoint_CombinedImageSamplers);
      continue;
    }
  }
  for (const auto& b : info.buffers) {
    if (!IGL_DEBUG_VERIFY(b.descriptorSet == kBindPoint_Buffers)) {
      IGL_LOG_ERROR(
          "Missing descriptor set id for buffers: the shader should contain \"layout(set = "
          "%u, ...)\"",
          kBindPoint_Buffers);
      continue;
    }
  }
  for (const auto& i : info.images) {
    if (!IGL_DEBUG_VERIFY(i.descriptorSet == kBindPoint_StorageImages)) {
      IGL_LOG_ERROR(
          "Missing descriptor set id for storage images: the shader should contain \"layout(set = "
          "%u, ...)\"",
          kBindPoint_StorageImages);
      continue;
    }
  }
}

VkComponentMapping componentMappingToVkComponentMapping(const ComponentMapping& mapping) {
  auto swizzleToVkSwizzle = [](Swizzle swizzle) -> VkComponentSwizzle {
    switch (swizzle) {
    case Swizzle_Default:
      return VK_COMPONENT_SWIZZLE_IDENTITY;
    case Swizzle_0:
      return VK_COMPONENT_SWIZZLE_ZERO;
    case Swizzle_1:
      return VK_COMPONENT_SWIZZLE_ONE;
    case Swizzle_R:
      return VK_COMPONENT_SWIZZLE_R;
    case Swizzle_G:
      return VK_COMPONENT_SWIZZLE_G;
    case Swizzle_B:
      return VK_COMPONENT_SWIZZLE_B;
    case Swizzle_A:
      return VK_COMPONENT_SWIZZLE_A;
    default:
      IGL_DEBUG_ASSERT_NOT_REACHED();
      return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
  };

  return VkComponentMapping{
      .r = swizzleToVkSwizzle(mapping.r),
      .g = swizzleToVkSwizzle(mapping.g),
      .b = swizzleToVkSwizzle(mapping.b),
      .a = swizzleToVkSwizzle(mapping.a),
  };
}

uint32_t getNumImagePlanes(VkFormat format) {
  switch (format) {
  case VK_FORMAT_UNDEFINED:
    return 0;
  case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    return 2;
  case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    return 3;
  default:
    return 1;
  }
}

bool hasDepth(VkFormat format) {
  return (format == VK_FORMAT_D16_UNORM) || (format == VK_FORMAT_X8_D24_UNORM_PACK32) ||
         (format == VK_FORMAT_D32_SFLOAT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

bool hasStencil(VkFormat format) {
  return (format == VK_FORMAT_S8_UINT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

} // namespace igl::vulkan

namespace igl::vulkan::functions {

namespace {
PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr() {
#if defined(_WIN32)
  HMODULE lib = LoadLibraryA("vulkan-1.dll");
  if (!lib) {
    DWORD dw = GetLastError();
    LPVOID lpMsgBuf = nullptr;

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      dw,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR)&lpMsgBuf,
                      0,
                      NULL) != 0) {
      IGL_LOG_ERROR("Failed to open vulkan-1.dll: %s\n", (LPCTSTR)lpMsgBuf);
      LocalFree(lpMsgBuf);
    } else {
      IGL_LOG_ERROR("Failed to open vulkan-1.dll");
    }

    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)GetProcAddress(lib, "vkGetInstanceProcAddr");
#elif defined(__APPLE__)
  void* lib = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
  IGL_LOG_INFO("Loading libvulkan.dylib\n");
  if (!lib) {
    IGL_LOG_INFO("Opening libvulkan.dylib failed: %s. Loading libvulkan.1.dylib instead\n",
                 dlerror());
    lib = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!lib) {
    IGL_LOG_INFO("Opening libvulkan.1.dylib failed: %s. Loading libMoltenVK.dylib instead\n",
                 dlerror());
    lib = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  }
  if (!lib) {
    IGL_LOG_ERROR("Failed to open libMoltenVK.dylib: %s\n", dlerror());
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
#else
  // Preload libraries that Vulkan ICD drivers commonly depend on.
  // This ensures they're available when the Vulkan loader dlopens() ICD drivers.
  // Libraries must be preloaded in dependency order (leaf dependencies first).
  // This is required because Buck2 uses a custom dynamic linker that doesn't search
  // standard system library paths (/lib64, /usr/lib64). We cannot use LD_LIBRARY_PATH
  // or RPATH because they would interfere with Buck2's hermetic build environment.
#if IGL_PLATFORM_LINUX && !defined(IGL_CMAKE_BUILD)
  const std::array<const char*, 27> kPreloadLibs = {
      // Base system libraries (leaf dependencies)
      "/lib64/libtinfo.so.6", // Required by libedit
      "/lib64/liblzma.so.5", // Required by libxml2
      "/lib64/libz.so.1", // Required by libLLVM, libxml2, Intel drivers
      "/usr/lib64/libzstd.so.1", // Required by libLLVM, Intel drivers
      "/usr/lib64/libffi.so.8", // Required by libLLVM
      "/lib64/libelf.so.1", // Required by Radeon driver
      // Mid-level dependencies
      "/lib64/libedit.so.0", // Required by libLLVM (depends on libtinfo)
      "/lib64/libxml2.so.2", // Required by libLLVM (depends on liblzma, libz)
      "/lib64/libexpat.so.1", // Required by Mesa drivers
      "/lib64/libXau.so.6", // Required by libxcb
      // X11/XCB libraries (for Intel and other hardware drivers)
      "/lib64/libxcb.so.1", // Required by Mesa drivers (depends on libXau)
      "/lib64/libxcb-randr.so.0", // Required by Lavapipe and all drivers
      "/lib64/libxcb-present.so.0", // Required by all Mesa Vulkan drivers
      "/lib64/libxcb-sync.so.1", // Required by Mesa drivers
      "/lib64/libxcb-xfixes.so.0", // Required by Mesa drivers
      "/lib64/libxcb-shm.so.0", // Required by Mesa drivers
      "/lib64/libX11-xcb.so.1", // Required by Intel drivers
      "/lib64/libxshmfence.so.1", // Required by Intel drivers
      "/lib64/libwayland-client.so.0", // Required by Intel drivers
      // DRM libraries
      "/lib64/libdrm.so.2", // Required by all hardware drivers
      "/usr/lib64/libdrm_amdgpu.so.1", // Required by Radeon driver
      // High-level dependencies
      "/lib64/libLLVM.so.20.1", // Required by Lavapipe and Radeon drivers
      "/lib64/libSPIRV-Tools.so", // Required by Lavapipe
      // Additional X11 libraries for Intel drivers
      "/lib64/libxcb-dri3.so.0", // Required by Intel drivers
  };
  for (const char* preload : kPreloadLibs) {
    const void* handle = dlopen(preload, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    if (handle) {
      IGL_LOG_DEBUG("IGL/Vulkan: preloaded `%s`.\n", preload);
    } else {
      // Log but continue - not all systems will have all drivers
      IGL_LOG_DEBUG("IGL/Vulkan: failed to preload `%s`: %s (not critical).\n", preload, dlerror());
    }
  }
#endif // IGL_PLATFORM_LINUX && !defined(IGL_CMAKE_BUILD)
  const std::array<const char*, 4> libs = {
      "libvulkan.so.1",
      "libvulkan.so",
      "/lib64/libvulkan.so.1",
      "/lib64/libvulkan.so",
  };
  void* lib = nullptr;
  for (const char* name : libs) {
    lib = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (lib) {
      break;
    }
    IGL_LOG_INFO("IGL/Vulkan: opening `%s` failed: %s.\n", name, dlerror());
  }

  if (!lib) {
    IGL_LOG_ERROR("IGL/Vulkan: no Vulkan library was found.\n");
    return nullptr;
  }
  return (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
#endif
}
} // namespace

void initialize(VulkanFunctionTable& table) {
  table.vkGetInstanceProcAddr = getVkGetInstanceProcAddr();

  if (!loadVulkanLoaderFunctions(&table, table.vkGetInstanceProcAddr)) {
    IGL_LOG_ERROR("Failed to load Vulkan loader functions");
    abort();
  }
}

void loadInstanceFunctions(VulkanFunctionTable& table,
                           VkInstance instance,
                           bool enableExtDebugUtils) {
  IGL_DEBUG_ASSERT(table.vkGetInstanceProcAddr != nullptr);
  loadVulkanInstanceFunctions(
      &table, instance, table.vkGetInstanceProcAddr, enableExtDebugUtils ? VK_TRUE : VK_FALSE);
}

void loadDeviceFunctions(VulkanFunctionTable& table, VkDevice device) {
  IGL_DEBUG_ASSERT(table.vkGetDeviceProcAddr != nullptr);
  loadVulkanDeviceFunctions(&table, device, table.vkGetDeviceProcAddr);
}

} // namespace igl::vulkan::functions
