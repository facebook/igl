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
};

} // namespace vulkan
} // namespace lvk
