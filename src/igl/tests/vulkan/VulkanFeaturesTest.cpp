/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanFeatures.h>

#include <igl/vulkan/Common.h>

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

// Copy with Same Config **************************************************************
TEST_F(VulkanFeaturesTest, CopyPerformed) {
  const igl::vulkan::VulkanContextConfig config;

  igl::vulkan::VulkanFeatures featuresSrc(config);
  featuresSrc.vkPhysicalDeviceFeatures2.features.dualSrcBlend = VK_FALSE;
  featuresSrc.featuresMultiview.multiview = VK_FALSE;
  featuresSrc.featuresSynchronization2.synchronization2 = VK_FALSE;

  igl::vulkan::VulkanFeatures featuresDst(config);
  EXPECT_TRUE(featuresDst.vkPhysicalDeviceFeatures2.features.dualSrcBlend);
  EXPECT_TRUE(featuresDst.featuresMultiview.multiview);
  EXPECT_TRUE(featuresDst.featuresSynchronization2.synchronization2);

  featuresDst = featuresSrc;

  EXPECT_FALSE(featuresDst.vkPhysicalDeviceFeatures2.features.dualSrcBlend);
  EXPECT_FALSE(featuresDst.featuresMultiview.multiview);
  EXPECT_FALSE(featuresDst.featuresSynchronization2.synchronization2);
}

// Self-Assignment ******************************************************************
TEST_F(VulkanFeaturesTest, SelfAssignment) {
  const igl::vulkan::VulkanContextConfig config;

  igl::vulkan::VulkanFeatures features(config);
  const VkBool32 originalDualSrcBlend = features.vkPhysicalDeviceFeatures2.features.dualSrcBlend;

  igl::vulkan::VulkanFeatures& ref = features;
  features = ref;

  EXPECT_EQ(features.vkPhysicalDeviceFeatures2.features.dualSrcBlend, originalDualSrcBlend);
}

// Check Selected Features ****************************************************
TEST_F(VulkanFeaturesTest, CheckSelectedFeatures_AllPresent) {
  const igl::vulkan::VulkanContextConfig config;

  const igl::vulkan::VulkanFeatures requested(config);
  const igl::vulkan::VulkanFeatures available(config);

  const igl::Result result = requested.checkSelectedFeatures(available);
  EXPECT_TRUE(result.isOk());
}

// Extension Bool Defaults ****************************************************
TEST_F(VulkanFeaturesTest, ExtensionBoolsDefaultToFalse) {
  const igl::vulkan::VulkanContextConfig config;
  const igl::vulkan::VulkanFeatures features(config);

  EXPECT_FALSE(features.has_VK_EXT_descriptor_buffer);
  EXPECT_FALSE(features.has_VK_EXT_descriptor_indexing);
  EXPECT_FALSE(features.has_VK_EXT_fragment_density_map);
  EXPECT_FALSE(features.has_VK_EXT_headless_surface);
  EXPECT_FALSE(features.has_VK_EXT_index_type_uint8);
  EXPECT_FALSE(features.has_VK_EXT_mesh_shader);
  EXPECT_FALSE(features.has_VK_EXT_queue_family_foreign);
  EXPECT_FALSE(features.has_VK_KHR_8bit_storage);
  EXPECT_FALSE(features.has_VK_KHR_buffer_device_address);
  EXPECT_FALSE(features.has_VK_KHR_get_surface_capabilities2);
  EXPECT_FALSE(features.has_VK_KHR_portability_enumeration);
  EXPECT_FALSE(features.has_VK_KHR_shader_non_semantic_info);
  EXPECT_FALSE(features.has_VK_KHR_synchronization2);
  EXPECT_FALSE(features.has_VK_KHR_timeline_semaphore);
  EXPECT_FALSE(features.has_VK_KHR_uniform_buffer_standard_layout);
  EXPECT_FALSE(features.has_VK_KHR_vulkan_memory_model);
  EXPECT_FALSE(features.has_VK_QCOM_multiview_per_view_viewports);
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
#if IGL_PLATFORM_ANDROID
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

TEST_F(VulkanFeaturesTest, CheckSelectedFeatures_MissingCoreFeature) {
  igl::setDebugBreakEnabled(false);

  const igl::vulkan::VulkanContextConfig config;

  const igl::vulkan::VulkanFeatures requested(config);
  igl::vulkan::VulkanFeatures available(config);
  available.vkPhysicalDeviceFeatures2.features.dualSrcBlend = VK_FALSE;

  const igl::Result result = requested.checkSelectedFeatures(available);
#if IGL_PLATFORM_APPLE
  EXPECT_TRUE(result.isOk());
#else
  EXPECT_FALSE(result.isOk());
#endif
}

TEST_F(VulkanFeaturesTest, CheckSelectedFeatures_MissingMultiview) {
  igl::setDebugBreakEnabled(false);

  const igl::vulkan::VulkanContextConfig config;

  const igl::vulkan::VulkanFeatures requested(config);
  igl::vulkan::VulkanFeatures available(config);
  available.featuresMultiview.multiview = VK_FALSE;

  const igl::Result result = requested.checkSelectedFeatures(available);
#if IGL_PLATFORM_APPLE
  EXPECT_TRUE(result.isOk());
#else
  EXPECT_FALSE(result.isOk());
#endif
}

TEST_F(VulkanFeaturesTest, ConfigDisablesDualSrcBlend) {
  igl::vulkan::VulkanContextConfig config;
  config.enableDualSrcBlend = false;

  const igl::vulkan::VulkanFeatures features(config);

  EXPECT_FALSE(features.vkPhysicalDeviceFeatures2.features.dualSrcBlend);
}

TEST_F(VulkanFeaturesTest, ConfigDisablesShaderInt16) {
  igl::vulkan::VulkanContextConfig config;
  config.enableShaderInt16 = false;

  const igl::vulkan::VulkanFeatures features(config);

  EXPECT_FALSE(features.vkPhysicalDeviceFeatures2.features.shaderInt16);
}

TEST_F(VulkanFeaturesTest, CheckSelectedFeatures_DescriptorIndexingEnabled_AllPresent) {
  igl::vulkan::VulkanContextConfig config;
  config.enableDescriptorIndexing = true;

  const igl::vulkan::VulkanFeatures requested(config);
  const igl::vulkan::VulkanFeatures available(config);

  const igl::Result result = requested.checkSelectedFeatures(available);
  EXPECT_TRUE(result.isOk());
}

} // namespace igl::tests
