/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanHelpers.h>
#include <memory>

namespace igl {
namespace vulkan {

class VulkanPipelineLayout;

class VulkanDescriptorSetLayout final {
 public:
  VulkanDescriptorSetLayout(VkDevice device,
                            uint32_t numBindings,
                            const VkDescriptorSetLayoutBinding* bindings,
                            const VkDescriptorBindingFlags* bindingFlags,
                            const char* debugName = nullptr);
  ~VulkanDescriptorSetLayout();

  VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
  VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

  VkDescriptorSetLayout getVkDescriptorSetLayout() const {
    return vkDescriptorSetLayout_;
  }

 public:
  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout vkDescriptorSetLayout_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
