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

#include <igl/Macros.h>
#include <volk.h>

#include <igl/Common.h>
#include <igl/Texture.h>
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

namespace igl::vulkan {

Result getResultFromVkResult(VkResult result);
void setResultFrom(Result* outResult, VkResult result);
VkFormat textureFormatToVkFormat(igl::TextureFormat format);
igl::TextureFormat vkFormatToTextureFormat(VkFormat format);
uint32_t getBytesPerPixel(VkFormat format);
igl::ColorSpace vkColorSpaceToColorSpace(VkColorSpaceKHR colorSpace);
VkMemoryPropertyFlags resourceStorageToVkMemoryPropertyFlags(igl::ResourceStorage resourceStorage);
VkCompareOp compareOpToVkCompareOp(igl::CompareOp func);
VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples);
VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(igl::ColorSpace colorSpace, bool isBGR = false);

} // namespace igl::vulkan
