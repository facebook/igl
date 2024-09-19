/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/TextureFormat.h>
#include <igl/vulkan/util/TextureFormatInt.h>

#define IGL_COMMON_SKIP_CHECK
#include <igl/Assert.h>

namespace igl::vulkan::util {

using TextureFormat = ::igl::TextureFormat;

TextureFormat intVkTextureFormatToTextureFormat(int32_t vkFormat) {
  return vkTextureFormatToTextureFormat(static_cast<VkFormat>(vkFormat));
}

} // namespace igl::vulkan::util
