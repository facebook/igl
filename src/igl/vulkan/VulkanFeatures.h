/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl::vulkan {

struct VulkanContextConfig;
class VulkanContext;

/// @brief Creates and maintains a list of feature structures for checking feature availability and
/// feature selection. This class provides a way to quickly enable the default - and required -
/// features for Vulkan 1.1. For the list of the required features, see method
/// enableDefaultFeatures1_1()
class VulkanFeatures final {
 public:
  explicit VulkanFeatures(uint32_t version, VulkanContextConfig config) noexcept;

  /// @brief Populates the VkPhysicalDeviceFeatures2 and its pNext chain for a Vulkan context
  void populateWithAvailablePhysicalDeviceFeatures(const VulkanContext& context,
                                                   VkPhysicalDevice physicalDevice) noexcept;

  /// @brief Enables the default features for Vulkan 1.1 required by IGL.
  /// The default features are:
  ///
  /// VULKAN 1.1:
  /// vkPhysicalDeviceFeatures2.features.dualSrcBlend
  /// vkPhysicalDeviceFeatures2.features.multiDrawIndirect
  /// vkPhysicalDeviceFeatures2.features.drawIndirectFirstInstance
  /// vkPhysicalDeviceFeatures2.features.depthBiasClamp
  /// vkPhysicalDeviceFeatures2.features.fillModeNonSolid
  /// vkPhysicalDeviceFeatures2.features.shaderInt16
  ///
  /// VkPhysicalDevice16BitStorageFeatures.storageBuffer16BitAccess
  ///
  /// VkPhysicalDeviceMultiviewFeatures.multiview
  ///
  /// VkPhysicalDeviceSamplerYcbcrConversionFeatures.samplerYcbcrConversion
  ///
  /// VkPhysicalDeviceShaderDrawParametersFeatures.shaderDrawParameters
  ///
  /// If VulkanContextConfig::enableDescriptorIndex is enabled:
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.shaderSampledImageArrayNonUniformIndexing
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUniformBufferUpdateAfterBind
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingSampledImageUpdateAfterBind
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageImageUpdateAfterBind
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageBufferUpdateAfterBind
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUpdateUnusedWhilePending
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingPartiallyBound
  ///   VkPhysicalDeviceDescriptorIndexingFeaturesEXT.runtimeDescriptorArray
  ///
  /// If VulkanContextConfig::enableBufferDeviceAddress is enabled:
  ///   VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress
  ///
  /// VULKAN 1.2:
  /// VkPhysicalDeviceShaderFloat16Int8Features.shaderFloat16
  void enableDefaultFeatures1_1() noexcept;

  /// @brief Checks the features enabled in this class against the ones passed
  /// in as a parameter in 'availableFeatures'. If a requested feature is not present, the class
  /// logs the message and returns a failure
  [[nodiscard]] igl::Result checkSelectedFeatures(
      const VulkanFeatures& availableFeatures) const noexcept;

  FOLLY_PUSH_WARNING
  FOLLY_GNU_DISABLE_WARNING("-Wmissing-field-initializers")
  // Vulkan 1.1
  VkPhysicalDeviceFeatures2 VkPhysicalDeviceFeatures2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      nullptr};
  VkPhysicalDeviceSamplerYcbcrConversionFeatures VkPhysicalDeviceSamplerYcbcrConversionFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
      nullptr};
  VkPhysicalDeviceShaderDrawParametersFeatures VkPhysicalDeviceShaderDrawParametersFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
      nullptr};
  VkPhysicalDeviceMultiviewFeatures VkPhysicalDeviceMultiviewFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      nullptr};
#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
      nullptr};
#endif
#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT VkPhysicalDeviceDescriptorIndexingFeaturesEXT_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
      nullptr};
#endif
  VkPhysicalDevice16BitStorageFeatures VkPhysicalDevice16BitStorageFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
      nullptr};

  // Vulkan 1.2
#if defined(VK_VERSION_1_2)
  VkPhysicalDeviceShaderFloat16Int8Features VkPhysicalDeviceShaderFloat16Int8Features_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
      nullptr};
#endif
  FOLLY_POP_WARNING

  // Assignment operator. We need to reassemble the feature chain because of the
  // pNext pointers
  VulkanFeatures& operator=(const VulkanFeatures& other) noexcept;

 public:
  // A copy of the config used by the VulkanContext
  VulkanContextConfig config_;

  // Stores the API version
  uint32_t version_ = 0;

 private:
  /// @brief Assembles the feature chain for the VkPhysicalDeviceFeatures2 structure by connecting
  /// the existing/required feature structures and their pNext chain.
  void assembleFeatureChain(const VulkanContextConfig& config) noexcept;
};

} // namespace igl::vulkan
