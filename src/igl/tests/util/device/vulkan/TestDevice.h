/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

namespace igl {
class IDevice;
namespace vulkan {
struct VulkanContextConfig;
} // namespace vulkan

namespace tests::util::device::vulkan {

/**
 Configure and return a context configuration.
 */
igl::vulkan::VulkanContextConfig getContextConfig(bool enableValidation = true);

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::shared_ptr<::igl::IDevice> createTestDevice(const igl::vulkan::VulkanContextConfig& config);

/**
 Helper to create a Vulkan device with default configuration and optional validation.
 */
std::shared_ptr<::igl::IDevice> createTestDevice(bool enableValidation = true);

} // namespace tests::util::device::vulkan
} // namespace igl
