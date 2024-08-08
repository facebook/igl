/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

/* The following defines tell VMA to load Vulkan functions dynamically
 * For this to work, we need to provide pointers to vkGetInstanceProcAddr and vkGetDeviceProcAddr to
 * VMA using the functions VmaVulkanFunctions::vkGetInstanceProcAddr and
 * VmaVulkanFunctions::vkGetDeviceProcAddr
 */
#undef VMA_STATIC_VULKAN_FUNCTIONS
#undef VMA_DYNAMIC_VULKAN_FUNCTIONS
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

/* Do not load Vulkan function prototypes, as we are loading the functions dynamically using Volk */
#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif // !defined(VK_NO_PROTOTYPES)
#if defined(IGL_CMAKE_BUILD)

/* Configuration defines for vk_mem_alloc.h */
#define VMA_VULKAN_VERSION 1002000

#include <vk_mem_alloc.h>
#else
#include <vk_mem_alloc_instantiated.h>
#endif // IGL_CMAKE_BUILD
