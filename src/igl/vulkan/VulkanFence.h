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
 * @brief Fences are used to synchronize CPU-GPU tasks. The VulkanFence class encapsulates the
 * creation and destruction of a vulkan fence object (VkFence). It stores an opaque handle for a
 * newly created fence object and for a device object.
 */
class VulkanFence final {
 public:
  VulkanFence(const VulkanFunctionTable& vf,
              VkDevice device,
              VkFlags flags,
              bool exportable = false,
              const char* debugName = nullptr);
  ~VulkanFence();

  VulkanFence(VulkanFence&& other) noexcept;
  VulkanFence& operator=(VulkanFence&& other) noexcept;

  VulkanFence(const VulkanFence&) = delete;
  VulkanFence operator=(const VulkanFence&) = delete;

  bool reset() noexcept;
  bool wait(uint64_t timeoutNs = UINT64_MAX) noexcept;

 public:
  const VulkanFunctionTable* vf_{};
  VkDevice device_ = VK_NULL_HANDLE;
  VkFence vkFence_ = VK_NULL_HANDLE;
  bool exportable_ = false;
};

} // namespace vulkan
} // namespace igl
