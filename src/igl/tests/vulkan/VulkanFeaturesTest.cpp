/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFeatures.h>

namespace igl::tests {

//
// VulkanFeaturesTest
//
// Unit tests for VulkanFeatures
//

// Constructing ***********************************************************************
class VulkanFeaturesTest : public ::testing::Test {};

TEST_F(VulkanFeaturesTest, Construct) {
  const igl::vulkan::VulkanContextConfig config;

  const igl::vulkan::VulkanFeatures features(config);

  EXPECT_EQ(features.vkPhysicalDeviceShaderFloat16Int8Features_.pNext, nullptr);
}

// Copying ***************************************************************************
TEST_F(VulkanFeaturesTest, CopyNotPerformed) {
  const igl::vulkan::VulkanContextConfig configSrc;
  EXPECT_FALSE(configSrc.enableDescriptorIndexing);

  igl::vulkan::VulkanContextConfig configDst;
  configDst.enableDescriptorIndexing = true;
  EXPECT_TRUE(configDst.enableDescriptorIndexing);

  const igl::vulkan::VulkanFeatures featuresSrc(configSrc);
  igl::vulkan::VulkanFeatures featuresDst(configDst);

  featuresDst = featuresSrc;

  // Unchanged
  EXPECT_TRUE(configDst.enableDescriptorIndexing);
}

// Enable Default Features ****************************************************
TEST_F(VulkanFeaturesTest, EnableDefaultFeatures) {
  const igl::vulkan::VulkanContextConfig config;

  igl::vulkan::VulkanFeatures features(config);

  EXPECT_FALSE(features.vkPhysicalDeviceShaderFloat16Int8Features_.shaderFloat16);
  EXPECT_FALSE(features.vkPhysicalDeviceShaderFloat16Int8Features_.shaderInt8);

  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2_.features.dualSrcBlend);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2_.features.shaderInt16);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2_.features.multiDrawIndirect);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2_.features.drawIndirectFirstInstance);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2_.features.depthBiasClamp);
#ifdef IGL_PLATFORM_ANDROID
  // fillModeNonSolid is not well supported on Android, only enable by default when it's not android
  EXPECT_FALSE(features.vkPhysicalDeviceFeatures2_.features.fillModeNonSolid);
#else
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2_.features.fillModeNonSolid);
#endif

  auto& descriptorIndexingFeatures = features.vkPhysicalDeviceDescriptorIndexingFeatures_;
  ASSERT_FALSE(config.enableDescriptorIndexing);
  EXPECT_TRUE(descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing);
  EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
  EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind);
  EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind);
  EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);
  EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending);
  EXPECT_TRUE(descriptorIndexingFeatures.descriptorBindingPartiallyBound);
  EXPECT_TRUE(descriptorIndexingFeatures.runtimeDescriptorArray);

  EXPECT_TRUE(features.vkPhysicalDevice16BitStorageFeatures_.storageBuffer16BitAccess);

  EXPECT_TRUE(features.vkPhysicalDeviceBufferDeviceAddressFeatures_.bufferDeviceAddress);
  EXPECT_TRUE(features.vkPhysicalDeviceMultiviewFeatures_.multiview);
  EXPECT_TRUE(features.vkPhysicalDeviceSamplerYcbcrConversionFeatures_.samplerYcbcrConversion);
  EXPECT_TRUE(features.vkPhysicalDeviceShaderDrawParametersFeatures_.shaderDrawParameters);
}

} // namespace igl::tests
