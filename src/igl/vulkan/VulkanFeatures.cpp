/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFeatures.h>

namespace igl::vulkan {

VulkanFeatures::VulkanFeatures(uint32_t version, VulkanContextConfig config) noexcept :
  config_(std::move(config)), version_(version) {
  assembleFeatureChain(config_);
}

void VulkanFeatures::populateWithAvailablePhysicalDeviceFeatures(
    const VulkanContext& context,
    VkPhysicalDevice physicalDevice) noexcept {
  IGL_DEBUG_ASSERT(context.vf_.vkGetPhysicalDeviceFeatures2 != nullptr,
                   "Pointer to function vkGetPhysicalDeviceFeatures2() is nullptr");

  context.vf_.vkGetPhysicalDeviceFeatures2(physicalDevice, &VkPhysicalDeviceFeatures2_);
}

void VulkanFeatures::enableDefaultFeatures1_1() noexcept {
  auto& features = VkPhysicalDeviceFeatures2_.features;
  features.dualSrcBlend = VK_TRUE;
  features.shaderInt16 = VK_TRUE;
  features.multiDrawIndirect = VK_TRUE;
  features.drawIndirectFirstInstance = VK_TRUE;
  features.depthBiasClamp = VK_TRUE;
#ifdef IGL_PLATFORM_ANDROID
  // fillModeNonSolid is not well supported on Android, only enable by default when it's not android
  features.fillModeNonSolid = VK_FALSE;
#else
  features.fillModeNonSolid = VK_TRUE;
#endif

#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  if (config_.enableDescriptorIndexing) {
    auto& descriptorIndexingFeatures = VkPhysicalDeviceDescriptorIndexingFeaturesEXT_;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
  }
#endif

  VkPhysicalDevice16BitStorageFeatures_.storageBuffer16BitAccess = VK_TRUE;

#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  if (config_.enableBufferDeviceAddress) {
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress = VK_TRUE;
  }
#endif
  VkPhysicalDeviceMultiviewFeatures_.multiview = VK_TRUE;
  VkPhysicalDeviceSamplerYcbcrConversionFeatures_.samplerYcbcrConversion = VK_TRUE;
  VkPhysicalDeviceShaderDrawParametersFeatures_.shaderDrawParameters = VK_TRUE;
}

igl::Result VulkanFeatures::checkSelectedFeatures(
    const VulkanFeatures& availableFeatures) const noexcept {
  IGL_DEBUG_ASSERT(availableFeatures.version_ == version_,
                   "The API versions don't match between the requested features and the ones that "
                   "are available");

  // Stores missing features
  std::string missingFeatures;

  // Macros for checking whether a requested feature is present. The macro logs an error if the
  // feature is requested and not available Based on
  // https://github.com/corporateshark/lightweightvk/blob/6b5ba5512f0e1ba7b20f4b37d7ec100eb25287c1/lvk/vulkan/VulkanClasses.cpp#L4702
#define ENABLE_VULKAN_FEATURE(requestedFeatureStruct, availableFeatureStruct, feature, version) \
  if ((requestedFeatureStruct.feature == VK_TRUE) &&                                            \
      (availableFeatureStruct.feature == VK_FALSE)) {                                           \
    missingFeatures.append("\n   " version " " #requestedFeatureStruct "." #feature);           \
  }

#define ENABLE_FEATURE_1_1(availableFeatureStruct, feature)                         \
  ENABLE_VULKAN_FEATURE(VkPhysicalDeviceFeatures2_.features,                        \
                        availableFeatureStruct.VkPhysicalDeviceFeatures2_.features, \
                        feature,                                                    \
                        "1.1")
  ENABLE_FEATURE_1_1(availableFeatures, dualSrcBlend)
  ENABLE_FEATURE_1_1(availableFeatures, shaderInt16)
  ENABLE_FEATURE_1_1(availableFeatures, multiDrawIndirect)
  ENABLE_FEATURE_1_1(availableFeatures, drawIndirectFirstInstance)
  ENABLE_FEATURE_1_1(availableFeatures, depthBiasClamp)
  ENABLE_FEATURE_1_1(availableFeatures, fillModeNonSolid)
#undef ENABLE_FEATURE_1_1

#define ENABLE_FEATURE_1_1_EXT(requestedFeatureStruct, availableFeatureStruct, feature) \
  ENABLE_VULKAN_FEATURE(requestedFeatureStruct, availableFeatureStruct, feature, "1.1 EXT")
#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  if (config_.enableDescriptorIndexing) {
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           shaderSampledImageArrayNonUniformIndexing)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           descriptorBindingUniformBufferUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           descriptorBindingSampledImageUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           descriptorBindingStorageImageUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           descriptorBindingStorageBufferUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           descriptorBindingUpdateUnusedWhilePending)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           descriptorBindingPartiallyBound)
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           availableFeatures.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_,
                           runtimeDescriptorArray)
  }
#endif
  ENABLE_FEATURE_1_1_EXT(VkPhysicalDevice16BitStorageFeatures_,
                         availableFeatures.VkPhysicalDevice16BitStorageFeatures_,
                         storageBuffer16BitAccess)
#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  if (config_.enableBufferDeviceAddress) {
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_,
                           availableFeatures.VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_,
                           bufferDeviceAddress)
  }
#endif
  ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceMultiviewFeatures_,
                         availableFeatures.VkPhysicalDeviceMultiviewFeatures_,
                         multiview)
  ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceSamplerYcbcrConversionFeatures_,
                         availableFeatures.VkPhysicalDeviceSamplerYcbcrConversionFeatures_,
                         samplerYcbcrConversion)
  ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceShaderDrawParametersFeatures_,
                         availableFeatures.VkPhysicalDeviceShaderDrawParametersFeatures_,
                         shaderDrawParameters)
#undef ENABLE_FEATURE_1_1_EXT

#if defined(VK_VERSION_1_2)
#define ENABLE_FEATURE_1_2_EXT(requestedFeatureStruct, availableFeatureStruct, feature) \
  ENABLE_VULKAN_FEATURE(requestedFeatureStruct, availableFeatureStruct, feature, "1.2")
  ENABLE_FEATURE_1_2_EXT(VkPhysicalDeviceShaderFloat16Int8Features_,
                         availableFeatures.VkPhysicalDeviceShaderFloat16Int8Features_,
                         shaderFloat16)
#undef ENABLE_FEATURE_1_2_EXT
#endif // VK_VERSION_1_2

#undef ENABLE_VULKAN_FEATURE

  if (!missingFeatures.empty()) {
#if !IGL_PLATFORM_APPLE
    IGL_DEBUG_ABORT("Missing Vulkan features: %s\n", missingFeatures.c_str());
    return Result(Result::Code::RuntimeError);
#else
    IGL_LOG_INFO("Missing Vulkan features: %s\n", missingFeatures.c_str());
    // For Vulkan 1.3 and MoltenVK, don't return an error as some 1.3 features are available
    // via extensions
#endif
  }

  // Return the value 'Ok'
  return igl::Result{};
}

void VulkanFeatures::assembleFeatureChain(const VulkanContextConfig& config) noexcept {
  // Versions 1.0 and 1.1 are always present

  // Reset all pNext pointers. We might be copying the chain from another VulkanFeatures object,
  // so we need to reset the pNext pointers to avoid dangling pointers. Some of the extensions'
  // pointers are guarded by #ifdefs below
  VkPhysicalDeviceFeatures2_.pNext = nullptr;
  VkPhysicalDeviceSamplerYcbcrConversionFeatures_.pNext = nullptr;
  VkPhysicalDeviceShaderDrawParametersFeatures_.pNext = nullptr;
  VkPhysicalDeviceMultiviewFeatures_.pNext = nullptr;

#if defined(VK_VERSION_1_2)
  VkPhysicalDeviceShaderFloat16Int8Features_.pNext = nullptr;
#endif // VK_VERSION_1_2

  // Add the required and optional features to the VkPhysicalDeviceFetaures2_
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceSamplerYcbcrConversionFeatures_);
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceShaderDrawParametersFeatures_);
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceMultiviewFeatures_);
#if defined(VK_VERSION_1_2)
  if (version_ >= VK_API_VERSION_1_2) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceShaderFloat16Int8Features_);
  }
#endif // VK_VERSION_1_2
#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.pNext = nullptr;
  if (config.enableBufferDeviceAddress) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_);
  }
#endif
#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT_.pNext = nullptr;
  if (config.enableDescriptorIndexing) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceDescriptorIndexingFeaturesEXT_);
  }
#endif
  VkPhysicalDevice16BitStorageFeatures_.pNext = nullptr;
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDevice16BitStorageFeatures_);
}

VulkanFeatures& VulkanFeatures::operator=(const VulkanFeatures& other) noexcept {
  if (this == &other) {
    return *this;
  }

  const bool sameVersion = version_ == other.version_;
  const bool sameConfiguration =
      config_.enableBufferDeviceAddress == other.config_.enableBufferDeviceAddress &&
      config_.enableDescriptorIndexing == other.config_.enableDescriptorIndexing;
  if (!sameVersion || !sameConfiguration) {
    return *this;
  }

  VkPhysicalDeviceFeatures2_ = other.VkPhysicalDeviceFeatures2_;

  VkPhysicalDeviceSamplerYcbcrConversionFeatures_ =
      other.VkPhysicalDeviceSamplerYcbcrConversionFeatures_;
  VkPhysicalDeviceShaderDrawParametersFeatures_ =
      other.VkPhysicalDeviceShaderDrawParametersFeatures_;
  VkPhysicalDeviceMultiviewFeatures_ = other.VkPhysicalDeviceMultiviewFeatures_;
#if defined(VK_KHR_buffer_device_address) && VK_KHR_buffer_device_address
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_ =
      other.VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_;
#endif
#if defined(VK_EXT_descriptor_indexing) && VK_EXT_descriptor_indexing
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT_ =
      other.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_;
#endif
  VkPhysicalDevice16BitStorageFeatures_ = other.VkPhysicalDevice16BitStorageFeatures_;

  // Vulkan 1.2
#if defined(VK_VERSION_1_2)
  VkPhysicalDeviceShaderFloat16Int8Features_ = other.VkPhysicalDeviceShaderFloat16Int8Features_;
#endif

  assembleFeatureChain(config_);

  return *this;
}

} // namespace igl::vulkan
