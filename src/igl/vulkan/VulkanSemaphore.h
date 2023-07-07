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
 * @brief Semaphores are used to synchronize GPU-GPU tasks. The VulkanSemaphore class encapsulates
 * the creation and destruction of a vulkan semaphore object (VkSemaphore). It stores an opaque
 * handle for a newly created semaphore object and for a device object.
 */
class VulkanSemaphore final {
 public:
  explicit VulkanSemaphore(VkDevice device, const char* debugName = nullptr);
  ~VulkanSemaphore();

  VulkanSemaphore(VulkanSemaphore&& other) noexcept;
  VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

  VulkanSemaphore(const VulkanSemaphore&) = delete;
  VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

 public:
  VkDevice device_ = VK_NULL_HANDLE;
  VkSemaphore vkSemaphore_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
