/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanHelpers.h>

namespace lvk {
namespace vulkan {

class VulkanContext;

/**
 * @brief Encapsulates a handle to a VkSampler and VkDevice used to create the resource. The class
 * also stores the sampler id, which is used for bindless rendering (see the ResourcesBinder and
 * VulkanContext classes for more information)
 */
class VulkanSampler final {
 public:
  /**
   * @brief Creates the VulkanSampler object and stores the opaque handler to it. The sampler is
   * created from the device based on the configuration passed as a parameter with a name that can
   * be used for debugging
   */
  VulkanSampler(const VulkanContext& ctx,
                VkDevice device,
                const VkSamplerCreateInfo& ci,
                const char* debugName = nullptr);
  ~VulkanSampler();

  VulkanSampler(const VulkanSampler&) = delete;
  VulkanSampler& operator=(const VulkanSampler&) = delete;

  /**
   * @brief Returns Vulkan's opaque handle to the sampler object
   */
  VkSampler getVkSampler() const {
    return vkSampler_;
  }

 public:
  const VulkanContext& ctx_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkSampler vkSampler_ = VK_NULL_HANDLE;
  /**
   * @brief The index into VulkanContext::samplers_. This index is intended to be used with bindless
   * rendering. Its value is set by the context when the resource is created and added to the vector
   * of samplers maintained by the VulkanContext.
   */
  uint32_t samplerId_ = 0;
};

} // namespace vulkan
} // namespace lvk
