/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/VulkanFunctionTable.h>

namespace igl::vulkan::functions {

void initialize(VulkanFunctionTable& table);

void loadInstanceFunctions(VulkanFunctionTable& table, VkInstance instance);

void loadDeviceFunctions(VulkanFunctionTable& table, VkDevice device);

} // namespace igl::vulkan::functions
