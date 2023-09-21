/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanDevice.h"

#include <igl/vulkan/Common.h>

igl::vulkan::VulkanDevice::VulkanDevice(const VulkanFunctionTable& vf,
                                        VkDevice device,
                                        const char* debugName) :
  vf_(vf), device_(device) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VK_ASSERT(
      ivkSetDebugObjectName(&vf, device_, VK_OBJECT_TYPE_DEVICE, (uint64_t)device_, debugName));
}

igl::vulkan::VulkanDevice::~VulkanDevice() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  vf_.vkDestroyDevice(device_, nullptr);
}
