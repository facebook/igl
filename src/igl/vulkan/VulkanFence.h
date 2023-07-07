/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl {
namespace vulkan {

/**
 * @brief Fences are used to synchronize CPU-GPU tasks. The VulkanFence class encapsulates the
 * creation and destruction of a vulkan fence object (VkFence). It stores an opaque handle for a
 * newly created fence object and for a device object.
 */
class VulkanFence final {
 public:
  VulkanFence(VkDevice device, VkFlags flags, const char* debugName = nullptr);
  ~VulkanFence();

  VulkanFence(VulkanFence&& other) noexcept;
  VulkanFence& operator=(VulkanFence&& other) noexcept;

  VulkanFence(const VulkanFence&) = delete;
  VulkanFence operator=(const VulkanFence&) = delete;

 public:
  VkDevice device_ = VK_NULL_HANDLE;
  VkFence vkFence_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
