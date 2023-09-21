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

class VulkanDevice final {
 public:
  explicit VulkanDevice(const VulkanFunctionTable& vf,
                        VkDevice device,
                        const char* debugName = nullptr);
  ~VulkanDevice();

  VulkanDevice(const VulkanDevice&) = delete;
  VulkanDevice& operator=(const VulkanDevice&) = delete;

  VkDevice getVkDevice() const {
    return device_;
  }

 public:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
