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
 * @brief A wrapper around a VkDevice.
 */
class VulkanDevice final {
 public:
  /** @brief Constructs a new VulkanDevice object from an existing VkDevice, which is automatically
   * destroyed when the VulkanDevice object is destroyed. The VulkanFunctionTable object must have
   * been initialized before being passed into the constructor. The `debugName` parameter can be
   * used to give the VkDevice a user-friendly name for debugging in tools such as RenderDoc, etc.
   */
  explicit VulkanDevice(const VulkanFunctionTable& vf,
                        VkDevice device,
                        const char* debugName = nullptr);
  ~VulkanDevice();

  VulkanDevice(const VulkanDevice&) = delete;
  VulkanDevice& operator=(const VulkanDevice&) = delete;

  [[nodiscard]] VkDevice getVkDevice() const {
    return device_;
  }

 public:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
};

} // namespace igl::vulkan
