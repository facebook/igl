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

#include <lvk/vulkan/VulkanUtils.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace lvk {
namespace vulkan {

void setResultFrom(Result* outResult, VkResult result) {
  if (!outResult) {
    return;
  }

  *outResult = getResultFromVkResult(result);
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
    return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_BT709_LINEAR_EXT};
  case lvk::ColorSpace_SRGB_NONLINEAR:
    [[fallthrough]];
  default:
    // default to normal sRGB non linear.
    return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
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
