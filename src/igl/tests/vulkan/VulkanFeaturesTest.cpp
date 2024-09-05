/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFeatures.h>
#include <string>
#include <vulkan/vulkan_core.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

namespace igl::tests {

//
// VulkanFeaturesTest
//
// Unit tests for VulkanFeatures
//

// Constructing ***********************************************************************
class VulkanFeaturesTest : public ::testing::Test {};

TEST_F(VulkanFeaturesTest, Construct_Version_1_1) {
  const igl::vulkan::VulkanContextConfig config_;

  const igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_1, config_);

  // We can only really test this is the SDK is at least version 1_2
#if defined(VK_VERSION_1_2)
  EXPECT_EQ(features.VkPhysicalDeviceShaderFloat16Int8Features_.pNext, nullptr);
#endif
}

TEST_F(VulkanFeaturesTest, Construct_Version_1_2) {
  const igl::vulkan::VulkanContextConfig config_;

  const igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_2, config_);

  // We can only really test this is the SDK is at least version 1_2
#if defined(VK_VERSION_1_2)
  EXPECT_NE(features.VkPhysicalDeviceShaderFloat16Int8Features_.pNext, nullptr);
#endif
}

} // namespace igl::tests
