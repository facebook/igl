/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/Texture.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl {
namespace vulkan {

class Buffer;
class SamplerState;
class Texture;

constexpr uint32_t kMaxBindingSlots = 16; // check shaders before changing this value

struct Slot {
  uint32_t texture = 0;
  uint32_t sampler = 0;
  VkDeviceAddress buffer = 0; // uvec2
  bool operator!=(const Slot& other) const {
    return texture != other.texture || sampler != other.sampler || buffer != other.buffer;
  }
};

static_assert(sizeof(Slot) == 4 * sizeof(uint32_t)); // sizeof(uvec4)

struct Bindings {
  Slot slots[kMaxBindingSlots] = {};

  // comparison operator and hash function for std::unordered_map<>
  bool operator==(const Bindings& other) const {
    for (uint32_t i = 0; i != kMaxBindingSlots; i++) {
      if (slots[i] != other.slots[i]) {
        return false;
      }
    }
    return true;
  }

  struct HashFunction {
    uint64_t operator()(const Bindings& s) const {
      uint64_t hash = 0;
      for (uint32_t i = 0; i != kMaxBindingSlots; i++) {
        hash ^= std::hash<uint32_t>()(s.slots[i].texture);
        hash ^= std::hash<uint32_t>()(s.slots[i].sampler);
        hash ^= std::hash<uint64_t>()(s.slots[i].buffer);
      }
      return hash;
    }
  };
};

static_assert(sizeof(Bindings) == 256);

class ResourcesBinder final {
 public:
  ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                  const VulkanContext& ctx,
                  VkPipelineBindPoint bindPoint);

  void bindBuffer(uint32_t index, igl::vulkan::Buffer* buffer, size_t bufferOffset);
  void bindSamplerState(uint32_t index, igl::vulkan::SamplerState* samplerState);
  void bindTexture(uint32_t index, igl::vulkan::Texture* tex);

  void updateBindings();
  void bindPipeline(VkPipeline pipeline);

 private:
  friend class VulkanContext;

  bool isGraphics() const {
    return bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS;
  }

 public:
  static constexpr uint32_t kDUBBufferSize = sizeof(Bindings);

 private:
  const VulkanContext& ctx_;
  VkCommandBuffer cmdBuffer_ = VK_NULL_HANDLE;
  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;
  bool isBindingsUpdateRequired_ = true;
  Bindings bindings_;
  VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace vulkan
} // namespace igl
