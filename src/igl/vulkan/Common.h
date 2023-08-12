/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>

// set to 1 to see very verbose debug console logs with Vulkan commands
#define LVK_VULKAN_PRINT_COMMANDS 0

#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES 1
#endif // !defined(VK_NO_PROTOTYPES)

#include <volk.h>

#include <lvk/LVK.h>
#include <igl/vulkan/VulkanHelpers.h>

// Enable to use VulkanMemoryAllocator (VMA)
#define LVK_VULKAN_USE_VMA 1

namespace lvk::vulkan {

void setResultFrom(Result* outResult, VkResult result);
bool isDepthOrStencilVkFormat(VkFormat format);
uint32_t getBytesPerPixel(VkFormat format);
VkMemoryPropertyFlags storageTypeToVkMemoryPropertyFlags(lvk::StorageType storage);
VkCompareOp compareOpToVkCompareOp(lvk::CompareOp func);
VkSampleCountFlagBits getVulkanSampleCountFlags(size_t numSamples);
VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(lvk::ColorSpace colorSpace, bool isBGR = false);

} // namespace lvk::vulkan
