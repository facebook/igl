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

  EXPECT_EQ(features.featuresShaderFloat16Int8.pNext, nullptr);
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

  EXPECT_FALSE(features.featuresShaderFloat16Int8.shaderFloat16);
  EXPECT_FALSE(features.featuresShaderFloat16Int8.shaderInt8);

  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2.features.dualSrcBlend);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2.features.shaderInt16);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2.features.multiDrawIndirect);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2.features.drawIndirectFirstInstance);
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2.features.depthBiasClamp);
#ifdef IGL_PLATFORM_ANDROID
  // fillModeNonSolid is not well supported on Android, only enable by default when it's not Android
  EXPECT_FALSE(features.vkPhysicalDeviceFeatures2.features.fillModeNonSolid);
#else
  EXPECT_TRUE(features.vkPhysicalDeviceFeatures2.features.fillModeNonSolid);
#endif

  {
    const auto& dif = features.featuresDescriptorIndexing;
    ASSERT_FALSE(config.enableDescriptorIndexing);
    EXPECT_TRUE(dif.shaderSampledImageArrayNonUniformIndexing);
    EXPECT_TRUE(dif.descriptorBindingUniformBufferUpdateAfterBind);
    EXPECT_TRUE(dif.descriptorBindingSampledImageUpdateAfterBind);
    EXPECT_TRUE(dif.descriptorBindingStorageImageUpdateAfterBind);
    EXPECT_TRUE(dif.descriptorBindingStorageBufferUpdateAfterBind);
    EXPECT_TRUE(dif.descriptorBindingUpdateUnusedWhilePending);
    EXPECT_TRUE(dif.descriptorBindingPartiallyBound);
    EXPECT_TRUE(dif.runtimeDescriptorArray);

    EXPECT_TRUE(features.features16BitStorage.storageBuffer16BitAccess);

    EXPECT_TRUE(features.featuresBufferDeviceAddress.bufferDeviceAddress);
    EXPECT_TRUE(features.featuresMultiview.multiview);
    EXPECT_TRUE(features.featuresSamplerYcbcrConversion.samplerYcbcrConversion);
    EXPECT_TRUE(features.featuresShaderDrawParameters.shaderDrawParameters);
  }
}

} // namespace igl::tests
