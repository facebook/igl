/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/vulkan/Device.h>

namespace igl::tests::util::device::vulkan {

/**
 Configure and return a context configuration.
 */
igl::vulkan::VulkanContextConfig getContextConfig(bool enableValidation = true);

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::shared_ptr<igl::vulkan::Device> createTestDevice(
    const igl::vulkan::VulkanContextConfig& config);

/**
 Helper to create a Vulkan device with default configuration and optional validation.
 */
std::shared_ptr<igl::vulkan::Device> createTestDevice(bool enableValidation = true);

} // namespace igl::tests::util::device::vulkan
