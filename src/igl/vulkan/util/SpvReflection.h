/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <igl/Texture.h>
#include <limits>
#include <vector>

namespace igl::vulkan::util {

constexpr uint32_t kNoBindingLocation = std::numeric_limits<uint32_t>::max();

struct TextureDescription {
  uint32_t bindingLocation = kNoBindingLocation;
  TextureType type = TextureType::Invalid;
};

struct SpvModuleInfo {
  std::vector<uint32_t> uniformBufferBindingLocations;
  std::vector<uint32_t> storageBufferBindingLocations;
  std::vector<TextureDescription> textures;
};

SpvModuleInfo getReflectionData(const uint32_t* words, size_t size);

} // namespace igl::vulkan::util
