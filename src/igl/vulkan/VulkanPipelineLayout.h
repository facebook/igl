/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanFunctions.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl {
namespace vulkan {

class VulkanPipelineLayout final {
 public:
  explicit VulkanPipelineLayout(const VulkanFunctionTable& vf,
                                VkDevice device,
                                const VkDescriptorSetLayout* layouts,
                                uint32_t numLayouts,
                                const VkPushConstantRange& range,
                                const char* debugName = nullptr);
  ~VulkanPipelineLayout();

  VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
  VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

  VkPipelineLayout getVkPipelineLayout() const {
    return vkPipelineLayout_;
  }

 public:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkPipelineLayout vkPipelineLayout_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
