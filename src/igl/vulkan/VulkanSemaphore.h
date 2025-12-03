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

namespace igl::vulkan {

/**
 * @brief Semaphores are used to synchronize GPU-GPU tasks. The VulkanSemaphore class encapsulates
 * the creation and destruction of a vulkan semaphore object (VkSemaphore). It stores an opaque
 * handle for a newly created semaphore object and for a device object.
 */
class VulkanSemaphore final {
 public:
  explicit VulkanSemaphore(const VulkanFunctionTable& vfIn,
                           VkDevice deviceIn,
                           bool exportableIn = false,
                           const char* debugName = nullptr);
  /// Create a timeline semaphore
  explicit VulkanSemaphore(const VulkanFunctionTable& vfIn,
                           VkDevice deviceIn,
                           uint64_t initialValue,
                           bool exportableIn = false,
                           const char* debugName = nullptr);
  ~VulkanSemaphore();

  VulkanSemaphore(VulkanSemaphore&& other) noexcept;
  VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

  VulkanSemaphore(const VulkanSemaphore&) = delete;
  VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

  [[nodiscard]] VkSemaphore getVkSemaphore() const noexcept;

  // Exportable semaphores are not used right now, so exclude from coverage
  // FIXME_DEPRECATED_COVERAGE_EXCLUDE_START
  [[nodiscard]] int getFileDescriptor() const noexcept;
  // FIXME_DEPRECATED_COVERAGE_EXCLUDE_END

 public:
  const VulkanFunctionTable* vf{};
  VkDevice device = VK_NULL_HANDLE;
  VkSemaphore vkSemaphore = VK_NULL_HANDLE;
  bool exportable = false;
};

} // namespace igl::vulkan
