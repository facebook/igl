/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFeatures.h>
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
  const igl::vulkan::VulkanContextConfig config;

  const igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_1, config);

  // We can only really test this is the SDK is at least version 1_2
#if defined(VK_VERSION_1_2)
  EXPECT_EQ(features.VkPhysicalDeviceShaderFloat16Int8Features_.pNext, nullptr);
#endif
}

TEST_F(VulkanFeaturesTest, Construct_Version_1_2) {
  const igl::vulkan::VulkanContextConfig config;

  const igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_2, config);

  // We can only really test this is the SDK is at least version 1_2
#if defined(VK_VERSION_1_2)
  EXPECT_NE(features.VkPhysicalDeviceShaderFloat16Int8Features_.pNext, nullptr);
#endif
}

// Copying ***************************************************************************
TEST_F(VulkanFeaturesTest, CopyNotPerformed) {
  const igl::vulkan::VulkanContextConfig configSrc;
  EXPECT_FALSE(configSrc.enableBufferDeviceAddress);
  EXPECT_FALSE(configSrc.enableDescriptorIndexing);

  igl::vulkan::VulkanContextConfig configDst;
  configDst.enableBufferDeviceAddress = true;
  configDst.enableDescriptorIndexing = true;
  EXPECT_TRUE(configDst.enableBufferDeviceAddress);
  EXPECT_TRUE(configDst.enableDescriptorIndexing);

  const igl::vulkan::VulkanFeatures featuresSrc(VK_API_VERSION_1_1, configSrc);
  igl::vulkan::VulkanFeatures featuresDst(VK_API_VERSION_1_2, configDst);

  featuresDst = featuresSrc;

  // Unchanged
  EXPECT_EQ(featuresDst.version_, VK_API_VERSION_1_2);
  EXPECT_TRUE(configDst.enableBufferDeviceAddress);
  EXPECT_TRUE(configDst.enableDescriptorIndexing);
}

// Enable Default Features Vk 1.1 ****************************************************
TEST_F(VulkanFeaturesTest, EnableDefaultFeatures_Vk_1_1) {
  const igl::vulkan::VulkanContextConfig config;

  igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_1, config);
  features.enableDefaultFeatures1_1();

  EXPECT_EQ(features.version_, VK_API_VERSION_1_1);
  EXPECT_FALSE(features.VkPhysicalDeviceShaderFloat16Int8Features_.shaderFloat16);
  EXPECT_FALSE(features.VkPhysicalDeviceShaderFloat16Int8Features_.shaderInt8);

  EXPECT_TRUE(features.VkPhysicalDeviceFeatures2_.features.dualSrcBlend);
  EXPECT_TRUE(features.VkPhysicalDeviceFeatures2_.features.shaderInt16);
  EXPECT_TRUE(features.VkPhysicalDeviceFeatures2_.features.multiDrawIndirect);
  EXPECT_TRUE(features.VkPhysicalDeviceFeatures2_.features.drawIndirectFirstInstance);
  EXPECT_TRUE(features.VkPhysicalDeviceFeatures2_.features.depthBiasClamp);
#ifdef IGL_PLATFORM_ANDROID
  // fillModeNonSolid is not well supported on Android, only enable by default when it's not android
  EXPECT_FALSE(features.VkPhysicalDeviceFeatures2_.features.fillModeNonSolid);
#else
  EXPECT_TRUE(features.VkPhysicalDeviceFeatures2_.features.fillModeNonSolid);
#endif

#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  if (config.enableDescriptorIndexing) {
    auto& descriptorIndexingFeatures = features.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_;
    EXPECT_TRUE(descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing);
    EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
    EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind);
    EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind);
    EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);
    EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending);
    EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingPartiallyBound);
    EXPECT_TRUE(descriptorIndexingFeatures.runtimeDescriptorArray);
  }
#endif

  EXPECT_TRUE(features.VkPhysicalDevice16BitStorageFeatures_.storageBuffer16BitAccess);

#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  if (config.enableBufferDeviceAddress) {
    EXPECT_TRUE(features.VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress);
  }
#endif
  EXPECT_TRUE(features.VkPhysicalDeviceMultiviewFeatures_.multiview);
  EXPECT_TRUE(features.VkPhysicalDeviceSamplerYcbcrConversionFeatures_.samplerYcbcrConversion);
  EXPECT_TRUE(features.VkPhysicalDeviceShaderDrawParametersFeatures_.shaderDrawParameters);
}

} // namespace igl::tests
