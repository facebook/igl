/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/Macros.h>
#include <igl/TextureFormat.h>

#if !defined(IGL_CMAKE_BUILD)
#include <vulkan/vulkan_core.h>
#else
#include <volk/volk.h>
#endif

namespace igl::vulkan::util {

/// Converts Vulkan texture format to an IGL TextureFormat.
/// @return The corresponding IGL format if known; otherwise returns TextureFormat::Invalid.
TextureFormat vkTextureFormatToTextureFormat(VkFormat vkFormat);

VkFormat invertRedAndBlue(VkFormat format);

inline bool isTextureFormatRGB(VkFormat format) {
  return format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB ||
         format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
}

inline bool isTextureFormatBGR(VkFormat format) {
  return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB ||
         format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
}
} // namespace igl::vulkan::util
