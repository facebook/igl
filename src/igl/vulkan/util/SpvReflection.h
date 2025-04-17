/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <igl/Texture.h>

namespace igl::vulkan::util {

constexpr uint32_t kNoBindingLocation = 0xffffffff;
constexpr uint32_t kNoDescriptorSet = 0xffffffff;

struct TextureDescription {
  uint32_t bindingLocation = kNoBindingLocation;
  uint32_t descriptorSet = kNoDescriptorSet;
  TextureType type = TextureType::Invalid;
};

struct ImageDescription {
  uint32_t bindingLocation = kNoBindingLocation;
  uint32_t descriptorSet = kNoDescriptorSet;
  TextureType type = TextureType::Invalid;
  uint32_t imageFormat = 0;
};

struct BufferDescription {
  uint32_t bindingLocation = kNoBindingLocation;
  uint32_t descriptorSet = kNoDescriptorSet;
  bool isStorage = false;
};

struct SpvModuleInfo {
  std::vector<BufferDescription> buffers;
  std::vector<TextureDescription> textures;
  std::vector<ImageDescription> images;
  bool hasPushConstants = false;
  uint32_t usageMaskBuffers = 0;
  uint32_t usageMaskTextures = 0;
};

SpvModuleInfo getReflectionData(const uint32_t* spirv, size_t numBytes);
SpvModuleInfo mergeReflectionData(const SpvModuleInfo& info1, const SpvModuleInfo& info2);

} // namespace igl::vulkan::util
