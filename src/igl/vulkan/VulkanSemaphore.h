/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFunctions.h>
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
  explicit VulkanSemaphore(const VulkanFunctionTable& vf,
                           VkDevice device,
                           const char* debugName = nullptr,
                           bool exportable = false);
  ~VulkanSemaphore();

  VulkanSemaphore(VulkanSemaphore&& other) noexcept;
  VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

  VulkanSemaphore(const VulkanSemaphore&) = delete;
  VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

  [[nodiscard]] VkSemaphore getVkSemaphore() const noexcept;

  [[nodiscard]] int getFileDescriptor() const noexcept;

 public:
  const VulkanFunctionTable* vf_{};
  VkDevice device_ = VK_NULL_HANDLE;
  VkSemaphore vkSemaphore_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
