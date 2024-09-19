/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/TextureFormat.h>

namespace iglu::textureloader::util {
igl::TextureFormat vkTextureFormatToTextureFormat(int32_t vkFormat);
} // namespace iglu::textureloader::util
