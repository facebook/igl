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
#include <igl/Common.h>

namespace igl::vulkan::util {

constexpr uint32_t kNoBindingLocation = 0xffffffff;
constexpr uint32_t kNoDescriptorSet = 0xffffffff;
constexpr uint32_t kNoInputAttachmentIndex = 0xffffffff;

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

/// A `subpassInput` variable. `inputAttachmentIndex` is the GLSL `input_attachment_index`, or
/// kNoInputAttachmentIndex if the variable has none. Arrays of subpass inputs and multisampled
/// subpass inputs (`subpassInputMS`) are not supported. They are reflected only so that they can be
/// rejected.
struct InputAttachmentDescription {
  uint32_t bindingLocation = kNoBindingLocation;
  uint32_t descriptorSet = kNoDescriptorSet;
  uint32_t inputAttachmentIndex = kNoInputAttachmentIndex;
  bool isArray = false;
  bool isMultisampled = false;
};

struct SpvModuleInfo {
  std::vector<BufferDescription> buffers;
  std::vector<TextureDescription> textures;
  std::vector<ImageDescription> images;
  std::vector<InputAttachmentDescription> inputAttachments;
  bool hasPushConstants = false;
  uint32_t usageMaskBuffers = 0;
  uint32_t usageMaskTextures = 0;
};

SpvModuleInfo getReflectionData(const uint32_t* spirv, size_t numBytes);
SpvModuleInfo mergeReflectionData(const SpvModuleInfo& info1, const SpvModuleInfo& info2);

} // namespace igl::vulkan::util
