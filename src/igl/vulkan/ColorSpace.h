/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <igl/ColorSpace.h>
#include <igl/vulkan/Common.h>

namespace igl::vulkan {
VkColorSpaceKHR colorSpaceToVkColorSpace(igl::ColorSpace colorSpace);
igl::ColorSpace vkColorSpaceToColorSpace(VkColorSpaceKHR colorSpace);
} // namespace igl::vulkan
