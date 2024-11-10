/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <memory>

namespace igl::vulkan {

/// @brief A wrapper around a VkDescriptorSetLayout
class VulkanDescriptorSetLayout final {
 public:
  /** @brief Construct a new VulkanDescriptorSetLayout object with the given context,
   * descriptor set layout create info, and optional debug name. `bindings` is a pointer to an
   * array of VkDescriptorSetLayoutBinding and `bindingFlags` is a pointer to an array of
   * VkDescriptorBindingFlags. The number of elements in each array must be equal to `numBindings`.
   */
  VulkanDescriptorSetLayout(const VulkanContext& ctx,
                            VkDescriptorSetLayoutCreateFlags flags,
                            uint32_t numBindings,
                            const VkDescriptorSetLayoutBinding* bindings,
                            const VkDescriptorBindingFlags* bindingFlags,
                            const char* debugName = nullptr);
  ~VulkanDescriptorSetLayout();

  VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
  VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

  [[nodiscard]] VkDescriptorSetLayout getVkDescriptorSetLayout() const {
    return vkDescriptorSetLayout_;
  }

 public:
  const VulkanContext& ctx_;
  VkDescriptorSetLayout vkDescriptorSetLayout_ = VK_NULL_HANDLE;
  uint32_t numBindings_ = 0;
};

} // namespace igl::vulkan
