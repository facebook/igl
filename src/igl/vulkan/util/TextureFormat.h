/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/TextureFormat.h>

namespace igl::vulkan::util {

/// Converts Vulkan texture format to an IGL TextureFormat.
/// @return The corresponding IGL format if known; otherwise returns TextureFormat::Invalid.
TextureFormat vkTextureFormatToTextureFormat(int32_t vkFormat);

} // namespace igl::vulkan::util
