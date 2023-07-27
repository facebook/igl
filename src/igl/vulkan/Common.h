/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>

// set to 1 to see very verbose debug console logs with Vulkan commands
#define IGL_VULKAN_PRINT_COMMANDS 0

#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES 1
#endif // !defined(VK_NO_PROTOTYPES)

#include <volk.h>

#include <lvk/LVK.h>
#include <igl/vulkan/VulkanHelpers.h>

// Enable to use VulkanMemoryAllocator (VMA)
#define IGL_VULKAN_USE_VMA 1

#define VK_ASSERT(func)                                            \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      LLOGW("Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
                    __FILE__,                                      \
                    __LINE__,                                      \
                    #func,                                         \
                    ivkGetVulkanResultString(vk_assert_result));   \
      assert(false);                                               \
    }                                                              \
  }

#define VK_ASSERT_RETURN(func)                                     \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      LLOGW("Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
                    __FILE__,                                      \
                    __LINE__,                                      \
                    #func,                                         \
                    ivkGetVulkanResultString(vk_assert_result));   \
      assert(false);                                               \
      return getResultFromVkResult(vk_assert_result);              \
    }                                                              \
  }

namespace lvk::vulkan {

Result getResultFromVkResult(VkResult result);
void setResultFrom(Result* outResult, VkResult result);
VkFormat textureFormatToVkFormat(lvk::TextureFormat format);
lvk::TextureFormat vkFormatToTextureFormat(VkFormat format);
bool isDepthOrStencilVkFormat(VkFormat format);
uint32_t getBytesPerPixel(VkFormat format);
VkMemoryPropertyFlags storageTypeToVkMemoryPropertyFlags(lvk::StorageType storage);
VkCompareOp compareOpToVkCompareOp(lvk::CompareOp func);
VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples);
VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(lvk::ColorSpace colorSpace, bool isBGR = false);

} // namespace lvk::vulkan
