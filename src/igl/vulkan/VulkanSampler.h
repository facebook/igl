/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

/**
 * @brief Encapsulates a handle to a VkSampler. The struct also stores the sampler id, which is used
 * for bindless rendering (see the ResourcesBinder and VulkanContext classes for more information)
 */
struct VulkanSampler final {
  VkSampler vkSampler_ = VK_NULL_HANDLE;
  /**
   * @brief The index into VulkanContext::samplers_. This index is intended to be used with bindless
   * rendering. Its value is set by the context when the resource is created and added to the vector
   * of samplers maintained by the VulkanContext.
   */
  uint32_t samplerId_ = 0;
};

} // namespace igl::vulkan
