/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFunctions.h>

namespace igl::vulkan {

/// @brief Wrapper around a Vulkan Command Pool (VkCommandPool)
class VulkanCommandPool final {
 public:
  /// @brief Creates a Vulkan Command Pool for a queue family with the specified creation flags and
  /// an optional debug name
  VulkanCommandPool(const VulkanFunctionTable& vf,
                    VkDevice device,
                    VkCommandPoolCreateFlags flags,
                    uint32_t queueFamilyIndex,
                    const char* debugName = nullptr);

  VulkanCommandPool(const VulkanCommandPool&) = delete;
  VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

  VulkanCommandPool(VulkanCommandPool&&) = delete;
  VulkanCommandPool& operator=(VulkanCommandPool&&) = delete;

  ~VulkanCommandPool();

  [[nodiscard]] VkCommandPool getVkCommandPool() const {
    return commandPool_;
  }

 private:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex_ = 0;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
};

} // namespace igl::vulkan
