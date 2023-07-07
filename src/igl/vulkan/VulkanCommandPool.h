/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>

namespace igl {
namespace vulkan {

class VulkanCommandPool final {
 public:
  VulkanCommandPool(VkDevice device,
                    VkCommandPoolCreateFlags flags,
                    uint32_t queueFamilyIndex,
                    const char* debugName = nullptr);

  VulkanCommandPool(const VulkanCommandPool&) = delete;
  VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

  VulkanCommandPool(VulkanCommandPool&&) = delete;
  VulkanCommandPool& operator=(VulkanCommandPool&&) = delete;

  ~VulkanCommandPool();

  VkCommandPool getVkCommandPool() const {
    return commandPool_;
  }

 private:
  VkDevice device_ = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex_ = 0;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
