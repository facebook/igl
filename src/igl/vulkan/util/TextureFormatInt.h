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

TextureFormat intVkTextureFormatToTextureFormat(int32_t vkFormat);

} // namespace igl::vulkan::util
