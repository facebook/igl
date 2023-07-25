/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanHelpers.h>

namespace lvk {
namespace vulkan {

class VulkanContext;

class VulkanSampler final {
 public:
  VulkanSampler() = default;
  VulkanSampler(VulkanContext* ctx,
                VkDevice device,
                const VkSamplerCreateInfo& ci,
                const char* debugName = nullptr);
  ~VulkanSampler();

  VulkanSampler(const VulkanSampler&) = delete;
  VulkanSampler& operator=(const VulkanSampler&) = delete;

  VulkanSampler(VulkanSampler&& other);
  VulkanSampler& operator=(VulkanSampler&& other);

  VkSampler getVkSampler() const {
    return vkSampler_;
  }

 public:
  VulkanContext* ctx_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;
  VkSampler vkSampler_ = VK_NULL_HANDLE;
  /**
   * @brief The index into VulkanContext::samplers_. This index is intended to be used with bindless
   * rendering. Its value is set by the context when the resource is created and added to the vector
   * of samplers maintained by the VulkanContext.
   */
  uint32_t samplerId_ = 0;
};

} // namespace vulkan
} // namespace lvk
