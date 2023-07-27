/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Common.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#include <windows.h>
#endif

#include <igl/vulkan/VulkanHelpers.h>

namespace lvk {
namespace vulkan {

Result getResultFromVkResult(VkResult result) {
  if (result == VK_SUCCESS) {
    return Result();
  }

  Result res(Result::Code::RuntimeError, ivkGetVulkanResultString(result));

  switch (result) {
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

VkFormat textureFormatToVkFormat(lvk::TextureFormat format) {
  using TextureFormat = ::lvk::TextureFormat;
  switch (format) {
  case TextureFormat::Invalid:
    return VK_FORMAT_UNDEFINED;
  case TextureFormat::R_UN8:
    return VK_FORMAT_R8_UNORM;
  case TextureFormat::R_UN16:
    return VK_FORMAT_R16_UNORM;
  case TextureFormat::R_F16:
    return VK_FORMAT_R16_SFLOAT;
  case TextureFormat::R_UI16:
    return VK_FORMAT_R16_UINT;
  case TextureFormat::RG_UN8:
    return VK_FORMAT_R8G8_UNORM;
  case TextureFormat::RG_UN16:
    return VK_FORMAT_R16G16_UNORM;
  case TextureFormat::BGRA_UN8:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case TextureFormat::RGBA_UN8:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case TextureFormat::RGBA_SRGB8:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case TextureFormat::BGRA_SRGB8:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case TextureFormat::RG_F16:
    return VK_FORMAT_R16G16_SFLOAT;
  case TextureFormat::RG_F32:
    return VK_FORMAT_R32G32_SFLOAT;
  case TextureFormat::RG_UI16:
    return VK_FORMAT_R16G16_UINT;
  case TextureFormat::R_F32:
    return VK_FORMAT_R32_SFLOAT;
  case TextureFormat::RGBA_F16:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case TextureFormat::RGBA_UI32:
    return VK_FORMAT_R32G32B32A32_UINT;
  case TextureFormat::RGBA_F32:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case TextureFormat::ETC2_RGB8:
    return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
  case TextureFormat::ETC2_SRGB8:
    return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
  case TextureFormat::BC7_RGBA:
    return VK_FORMAT_BC7_UNORM_BLOCK;
  case TextureFormat::Z_UN16:
    return VK_FORMAT_D16_UNORM;
  case TextureFormat::Z_UN24:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case TextureFormat::Z_F32:
    return VK_FORMAT_D32_SFLOAT;
  case TextureFormat::Z_UN24_S_UI8:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  default:
    IGL_ASSERT_MSG(false, "TextureFormat value not handled: %d", (int)format);
  }
#if defined(_MSC_VER)
  return VK_FORMAT_UNDEFINED;
#endif // _MSC_VER
}

lvk::TextureFormat vkFormatToTextureFormat(VkFormat format) {
  using TextureFormat = ::lvk::TextureFormat;
  switch (format) {
  case VK_FORMAT_UNDEFINED:
    return TextureFormat::Invalid;
  case VK_FORMAT_R8_UNORM:
    return TextureFormat::R_UN8;
  case VK_FORMAT_R16_UNORM:
    return TextureFormat::R_UN16;
  case VK_FORMAT_R16_SFLOAT:
    return TextureFormat::R_F16;
  case VK_FORMAT_R16_UINT:
    return TextureFormat::R_UI16;
  case VK_FORMAT_R8G8_UNORM:
    return TextureFormat::RG_UN8;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return TextureFormat::BGRA_UN8;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return TextureFormat::RGBA_UN8;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return TextureFormat::RGBA_SRGB8;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return TextureFormat::BGRA_SRGB8;
  case VK_FORMAT_R16G16_UNORM:
    return TextureFormat::RG_UN16;
  case VK_FORMAT_R16G16_SFLOAT:
    return TextureFormat::RG_F16;
  case VK_FORMAT_R32G32_SFLOAT:
    return TextureFormat::RG_F32;
  case VK_FORMAT_R16G16_UINT:
    return TextureFormat::RG_UI16;
  case VK_FORMAT_R32_SFLOAT:
    return TextureFormat::R_F32;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return TextureFormat::RGBA_F16;
  case VK_FORMAT_R32G32B32A32_UINT:
    return TextureFormat::RGBA_UI32;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return TextureFormat::RGBA_F32;
  case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
  case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    return TextureFormat::ETC2_RGB8;
  case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    return TextureFormat::ETC2_SRGB8;
  case VK_FORMAT_D16_UNORM:
    return TextureFormat::Z_UN16;
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return TextureFormat::BC7_RGBA;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return TextureFormat::Z_UN24;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return TextureFormat::Z_UN24_S_UI8;
  case VK_FORMAT_D32_SFLOAT:
    return TextureFormat::Z_F32;
  default:
    IGL_ASSERT_MSG(false, "VkFormat value not handled: %d", (int)format);
  }
#if defined(_MSC_VER)
  return TextureFormat::Invalid;
#endif // _MSC_VER
}

uint32_t getBytesPerPixel(VkFormat format) {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
    return 1;
  case VK_FORMAT_R16_SFLOAT:
    return 2;
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_B8G8R8_UNORM:
    return 3;
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R32_SFLOAT:
    return 4;
  case VK_FORMAT_R16G16B16_SFLOAT:
    return 6;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
    return 8;
  case VK_FORMAT_R32G32B32_SFLOAT:
    return 12;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return 16;
  default:
    IGL_ASSERT_MSG(false, "VkFormat value not handled: %d", (int)format);
  }
#if defined(_MSC_VER)
  return 1;
#endif // _MSC_VER
}

VkMemoryPropertyFlags storageTypeToVkMemoryPropertyFlags(lvk::StorageType storage) {
  VkMemoryPropertyFlags memFlags{0};

  switch (storage) {
  case StorageType_Device:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  case StorageType_HostVisible:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case StorageType_Memoryless:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    break;
  }

  return memFlags;
}

VkCompareOp compareOpToVkCompareOp(lvk::CompareOp func) {
  switch (func) {
  case lvk::CompareOp_Never:
    return VK_COMPARE_OP_NEVER;
  case lvk::CompareOp_Less:
    return VK_COMPARE_OP_LESS;
  case lvk::CompareOp_Equal:
    return VK_COMPARE_OP_EQUAL;
  case lvk::CompareOp_LessEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case lvk::CompareOp_Greater:
    return VK_COMPARE_OP_GREATER;
  case lvk::CompareOp_NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case lvk::CompareOp_GreaterEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case lvk::CompareOp_AlwaysPass:
    return VK_COMPARE_OP_ALWAYS;
  }
  IGL_ASSERT_MSG(false, "CompareFunction value not handled: %d", (int)func);
  return VK_COMPARE_OP_ALWAYS;
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

VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(lvk::ColorSpace colorSpace, bool isBGR) {
  switch (colorSpace) {
  case lvk::ColorSpace_SRGB_LINEAR:
    // the closest thing to sRGB linear
    return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM,
                              VK_COLOR_SPACE_BT709_LINEAR_EXT};
  case lvk::ColorSpace_SRGB_NONLINEAR:
    [[fallthrough]];
  default:
    // default to normal sRGB non linear.
    return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB,
                              VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  }
}

bool isDepthOrStencilVkFormat(VkFormat format) {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_S8_UINT:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
  return false;
}

} // namespace vulkan
} // namespace lvk
