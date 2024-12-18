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

  // Vulkan 1.1
  VkPhysicalDeviceFeatures2 VkPhysicalDeviceFeatures2_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  VkPhysicalDeviceSamplerYcbcrConversionFeatures VkPhysicalDeviceSamplerYcbcrConversionFeatures_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
  VkPhysicalDeviceShaderDrawParametersFeatures VkPhysicalDeviceShaderDrawParametersFeatures_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES};
  VkPhysicalDeviceMultiviewFeatures VkPhysicalDeviceMultiviewFeatures_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR};
#endif
#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT VkPhysicalDeviceDescriptorIndexingFeaturesEXT_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
#endif
  VkPhysicalDevice16BitStorageFeatures VkPhysicalDevice16BitStorageFeatures_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};

  // Vulkan 1.2
#if defined(VK_VERSION_1_2)
  VkPhysicalDeviceShaderFloat16Int8Features VkPhysicalDeviceShaderFloat16Int8Features_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR};
#endif
#if defined(VK_EXT_index_type_uint8) && VK_EXT_index_type_uint8
  VkPhysicalDeviceIndexTypeUint8FeaturesEXT VkPhysicalDeviceIndexTypeUint8Features_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT};
#endif

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
  bool hasExtension(const char* ext) const;

  std::vector<VkExtensionProperties> extensions_;
};

} // namespace igl::vulkan
