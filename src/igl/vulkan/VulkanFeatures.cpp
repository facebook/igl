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
  // Vulkan 1.1
  VkPhysicalDeviceFeatures2_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .features = {},
  }),
  VkPhysicalDeviceSamplerYcbcrConversionFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
      .samplerYcbcrConversion = VK_FALSE,
  }),
  VkPhysicalDeviceShaderDrawParametersFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
      .shaderDrawParameters = VK_FALSE,
  }),
  VkPhysicalDeviceMultiviewFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      .multiview = VK_FALSE,
      .multiviewGeometryShader = VK_FALSE,
      .multiviewTessellationShader = VK_FALSE,
  }),
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
      .bufferDeviceAddress = VK_TRUE,
      .bufferDeviceAddressCaptureReplay = VK_FALSE,
      .bufferDeviceAddressMultiDevice = VK_FALSE,
  }),
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
      .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
      .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
      .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
      .shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
      .shaderSampledImageArrayNonUniformIndexing = VK_FALSE,
      .shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
      .shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
      .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
      .shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
      .shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
      .descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
      .descriptorBindingSampledImageUpdateAfterBind = VK_FALSE,
      .descriptorBindingStorageImageUpdateAfterBind = VK_FALSE,
      .descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
      .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
      .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
      .descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
      .descriptorBindingPartiallyBound = VK_FALSE,
      .descriptorBindingVariableDescriptorCount = VK_FALSE,
      .runtimeDescriptorArray = VK_FALSE,
  }),
  VkPhysicalDevice16BitStorageFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
      .storageBuffer16BitAccess = VK_FALSE,
      .uniformAndStorageBuffer16BitAccess = VK_FALSE,
      .storagePushConstant16 = VK_FALSE,
      .storageInputOutput16 = VK_FALSE,
  }),
  // Vulkan 1.2
  VkPhysicalDeviceShaderFloat16Int8Features_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
      .shaderFloat16 = VK_FALSE,
      .shaderInt8 = VK_FALSE,
  }),
  VkPhysicalDeviceIndexTypeUint8Features_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
      .indexTypeUint8 = VK_FALSE,
  }),
  VkPhysicalDeviceSynchronization2Features_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
      .synchronization2 = VK_TRUE,
  }),
  VkPhysicalDeviceTimelineSemaphoreFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
      .timelineSemaphore = VK_TRUE,
  }),
  config_(config),
  version_(version) {
  extensions_.resize(kNumberOfExtensionTypes);
  enabledExtensions_.resize(kNumberOfExtensionTypes);

  // All the above get assembled into a feature chain
  assembleFeatureChain(config_);
}

void VulkanFeatures::populateWithAvailablePhysicalDeviceFeatures(
    const VulkanContext& context,
    VkPhysicalDevice physicalDevice) noexcept {
  IGL_DEBUG_ASSERT(context.vf_.vkGetPhysicalDeviceFeatures2 != nullptr,
                   "Pointer to function vkGetPhysicalDeviceFeatures2() is nullptr");
  uint32_t numExtensions = 0;
  context.vf_.vkEnumerateDeviceExtensionProperties(
      physicalDevice, nullptr, &numExtensions, nullptr);
  extensionProps_.resize(numExtensions);
  context.vf_.vkEnumerateDeviceExtensionProperties(
      physicalDevice, nullptr, &numExtensions, extensionProps_.data());
  assembleFeatureChain(context.config_);
  context.vf_.vkGetPhysicalDeviceFeatures2(physicalDevice, &VkPhysicalDeviceFeatures2_);
}

bool VulkanFeatures::hasExtension(const char* ext) const {
  for (const VkExtensionProperties& props : extensionProps_) {
    if (strcmp(ext, props.extensionName) == 0) {
      return true;
    }
  }
  return false;
}

void VulkanFeatures::enableDefaultFeatures() noexcept {
  auto& features = VkPhysicalDeviceFeatures2_.features;
  features.dualSrcBlend = config_.enableDualSrcBlend ? VK_TRUE : VK_FALSE;
  features.shaderInt16 = config_.enableShaderInt16 ? VK_TRUE : VK_FALSE;
  features.multiDrawIndirect = VK_TRUE;
  features.drawIndirectFirstInstance = VK_TRUE;
  features.depthBiasClamp = VK_TRUE;
#ifdef IGL_PLATFORM_ANDROID
  // fillModeNonSolid is not well supported on Android, only enable by default when it's not android
  features.fillModeNonSolid = VK_FALSE;
#else
  features.fillModeNonSolid = VK_TRUE;
#endif

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

  VkPhysicalDevice16BitStorageFeatures_.storageBuffer16BitAccess =
      config_.enableStorageBuffer16BitAccess ? VK_TRUE : VK_FALSE;

  if (config_.enableBufferDeviceAddress) {
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.bufferDeviceAddress = VK_TRUE;
  }
  VkPhysicalDeviceMultiviewFeatures_.multiview = VK_TRUE;
  VkPhysicalDeviceSamplerYcbcrConversionFeatures_.samplerYcbcrConversion = VK_TRUE;
  VkPhysicalDeviceShaderDrawParametersFeatures_.shaderDrawParameters =
      config_.enableShaderDrawParameters ? VK_TRUE : VK_FALSE;
  VkPhysicalDeviceSynchronization2Features_.synchronization2 = VK_TRUE;
  VkPhysicalDeviceTimelineSemaphoreFeatures_.timelineSemaphore = VK_TRUE;
}

Result VulkanFeatures::checkSelectedFeatures(
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
  ENABLE_FEATURE_1_1_EXT(VkPhysicalDevice16BitStorageFeatures_,
                         availableFeatures.VkPhysicalDevice16BitStorageFeatures_,
                         storageBuffer16BitAccess)
  if (config_.enableBufferDeviceAddress) {
    ENABLE_FEATURE_1_1_EXT(VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_,
                           availableFeatures.VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_,
                           bufferDeviceAddress)
  }
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

#define ENABLE_FEATURE_1_2_EXT(requestedFeatureStruct, availableFeatureStruct, feature) \
  ENABLE_VULKAN_FEATURE(requestedFeatureStruct, availableFeatureStruct, feature, "1.2")
  ENABLE_FEATURE_1_2_EXT(VkPhysicalDeviceShaderFloat16Int8Features_,
                         availableFeatures.VkPhysicalDeviceShaderFloat16Int8Features_,
                         shaderFloat16)
#undef ENABLE_FEATURE_1_2_EXT

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
  return Result{};
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
  VkPhysicalDeviceIndexTypeUint8Features_.pNext = nullptr;
  VkPhysicalDeviceSynchronization2Features_.pNext = nullptr;
  VkPhysicalDeviceTimelineSemaphoreFeatures_.pNext = nullptr;
  VkPhysicalDeviceShaderFloat16Int8Features_.pNext = nullptr;
  VkPhysicalDevice16BitStorageFeatures_.pNext = nullptr;
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_.pNext = nullptr;
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT_.pNext = nullptr;

  // Add the required and optional features to the VkPhysicalDeviceFetaures2_
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceSamplerYcbcrConversionFeatures_);
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceShaderDrawParametersFeatures_);
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceMultiviewFeatures_);
  if (version_ >= VK_API_VERSION_1_2) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceShaderFloat16Int8Features_);
  }
  if (hasExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_);
  }
  if (config.enableDescriptorIndexing) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceDescriptorIndexingFeaturesEXT_);
  }
  ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDevice16BitStorageFeatures_);

  if (hasExtension(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME)) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceIndexTypeUint8Features_);
  }
  if (hasExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceSynchronization2Features_);
  }
  if (hasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
    ivkAddNext(&VkPhysicalDeviceFeatures2_, &VkPhysicalDeviceTimelineSemaphoreFeatures_);
  }
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
  VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_ =
      other.VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_;
  VkPhysicalDeviceDescriptorIndexingFeaturesEXT_ =
      other.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_;
  VkPhysicalDevice16BitStorageFeatures_ = other.VkPhysicalDevice16BitStorageFeatures_;

  // Vulkan 1.2
  VkPhysicalDeviceShaderFloat16Int8Features_ = other.VkPhysicalDeviceShaderFloat16Int8Features_;
  VkPhysicalDeviceIndexTypeUint8Features_ = other.VkPhysicalDeviceIndexTypeUint8Features_;
  VkPhysicalDeviceSynchronization2Features_ = other.VkPhysicalDeviceSynchronization2Features_;
  VkPhysicalDeviceTimelineSemaphoreFeatures_ = other.VkPhysicalDeviceTimelineSemaphoreFeatures_;

  extensions_ = other.extensions_;
  enabledExtensions_ = other.enabledExtensions_;
  extensionProps_ = other.extensionProps_;

  assembleFeatureChain(config_);

  return *this;
}

void VulkanFeatures::enumerate(const VulkanFunctionTable& vf) {
  uint32_t count = 0;
  VK_ASSERT(vf.vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allExtensions(count);

  VK_ASSERT(vf.vkEnumerateInstanceExtensionProperties(nullptr, &count, allExtensions.data()));

  constexpr size_t vectorIndex = (size_t)ExtensionType::Instance;
  std::transform(allExtensions.cbegin(),
                 allExtensions.cend(),
                 std::back_inserter(extensions_[vectorIndex]),
                 [](const VkExtensionProperties& extensionProperties) {
                   return extensionProperties.extensionName;
                 });
}

void VulkanFeatures::enumerate(const VulkanFunctionTable& vf, VkPhysicalDevice device) {
  uint32_t count = 0;
  VK_ASSERT(vf.vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allExtensions(count);

  VK_ASSERT(vf.vkEnumerateDeviceExtensionProperties(device, nullptr, &count, allExtensions.data()));

  constexpr size_t vectorIndex = (size_t)ExtensionType::Device;
  std::transform(allExtensions.cbegin(),
                 allExtensions.cend(),
                 std::back_inserter(extensions_[vectorIndex]),
                 [](const VkExtensionProperties& extensionProperties) {
                   return extensionProperties.extensionName;
                 });
}

const std::vector<std::string>& VulkanFeatures::allAvailableExtensions(
    ExtensionType extensionType) const {
  const size_t vectorIndex = (size_t)extensionType;
  return extensions_[vectorIndex];
}

bool VulkanFeatures::available(const char* extensionName, ExtensionType extensionType) const {
  const size_t vectorIndex = (size_t)extensionType;
  const std::string extensionNameStr(extensionName);
  auto result = std::find_if(
      extensions_[vectorIndex].begin(),
      extensions_[vectorIndex].end(),
      [&extensionNameStr](const std::string& extension) { return extension == extensionNameStr; });

  return result != extensions_[vectorIndex].end();
}

bool VulkanFeatures::enable(const char* extensionName, ExtensionType extensionType) {
  const size_t vectorIndex = (size_t)extensionType;
  if (available(extensionName, extensionType)) {
    enabledExtensions_[vectorIndex].insert(extensionName);
    return true;
  }
  return false;
}

void VulkanFeatures::enableCommonInstanceExtensions(const VulkanContextConfig& config) {
  enable(VK_KHR_SURFACE_EXTENSION_NAME, ExtensionType::Instance);
  enable(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, ExtensionType::Instance);
  enable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ExtensionType::Instance);
#if IGL_PLATFORM_WINDOWS
  enable(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, ExtensionType::Instance);
#elif IGL_PLATFORM_ANDROID
  enable("VK_KHR_android_surface", ExtensionType::Instance);
#elif IGL_PLATFORM_LINUX
  enable("VK_KHR_xlib_surface", ExtensionType::Instance);
#elif IGL_PLATFORM_MACOSX
  enable(VK_EXT_METAL_SURFACE_EXTENSION_NAME, ExtensionType::Instance);
#endif

#if IGL_PLATFORM_MACOSX
  // https://vulkan.lunarg.com/doc/sdk/1.3.216.0/mac/getting_started.html
  if (!enable(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, ExtensionType::Instance)) {
    IGL_LOG_ERROR("VK_KHR_portability_enumeration extension not supported\n");
  }
#endif

#if !IGL_PLATFORM_ANDROID
  if (config.enableValidation) {
    enable(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, ExtensionType::Instance);
  }
#endif
  if (config.headless) {
    const bool enabledExtension =
        enable(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, ExtensionType::Instance);
    if (!enabledExtension) {
      IGL_LOG_ERROR("VK_EXT_headless_surface extension not supported");
    }
  }
  if (config.swapChainColorSpace != igl::ColorSpace::SRGB_NONLINEAR) {
    const bool enabledExtension =
        enable(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, ExtensionType::Instance);
    if (!enabledExtension) {
      IGL_LOG_ERROR("VK_EXT_swapchain_colorspace extension not supported\n");
    }
  }
}

void VulkanFeatures::enableCommonDeviceExtensions(const VulkanContextConfig& config) {
  enable(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, ExtensionType::Device);
#if IGL_PLATFORM_ANDROID
  enable(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME, ExtensionType::Device);
  if (config.enableDescriptorIndexing) {
#endif
    // On Android, vkEnumerateInstanceExtensionProperties crashes when validation layers are
    // enabled for DEBUG builds. https://issuetracker.google.com/issues/209835779?pli=1 Hence,
    // allow developers to not enable certain extensions on Android which are not present.
    enable(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, ExtensionType::Device);
#if IGL_PLATFORM_ANDROID
  }
#endif
  enable(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, ExtensionType::Device);
#if !IGL_PLATFORM_ANDROID || !IGL_DEBUG
  // On Android, vkEnumerateInstanceExtensionProperties crashes when validation layers are
  // enabled for DEBUG builds. https://issuetracker.google.com/issues/209835779?pli=1 Hence,
  // don't enable some extensions on Android which are not present and no way to check without
  // crashing.
  enable(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, ExtensionType::Device);
#endif // !IGL_PLATFORM_ANDROID || !IGL_DEBUG
  enable(VK_KHR_SWAPCHAIN_EXTENSION_NAME, ExtensionType::Device);

#if IGL_PLATFORM_MACOSX
  std::ignore = IGL_DEBUG_VERIFY(enable("VK_KHR_portability_subset", ExtensionType::Device));
#endif

#if IGL_PLATFORM_WINDOWS
  enable(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, ExtensionType::Device);
#endif // IGL_PLATFORM_WINDOWS

#if IGL_PLATFORM_LINUX
  enable(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, ExtensionType::Device);
#endif // IGL_PLATFORM_LINUX

#if defined(IGL_WITH_TRACY_GPU)
  enable(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, ExtensionType::Device);
#endif // IGL_WITH_TRACY_GPU

  has_VK_EXT_index_type_uint8 =
      enable(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_timeline_semaphore =
      enable(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, ExtensionType::Device);
  has_VK_KHR_synchronization2 =
      enable(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_buffer_device_address =
      enable(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, ExtensionType::Device);
}

bool VulkanFeatures::enabled(const char* extensionName) const {
  return (enabledExtensions_[(size_t)ExtensionType::Instance].count(extensionName) > 0) ||
         (enabledExtensions_[(size_t)ExtensionType::Device].count(extensionName) > 0);
}

std::vector<const char*> VulkanFeatures::allEnabled(ExtensionType extensionType) const {
  const size_t vectorIndex = (size_t)extensionType;
  std::vector<const char*> returnList;
  for (const auto& extension : enabledExtensions_[vectorIndex]) {
    returnList.emplace_back(extension.c_str());
  }
  return returnList;
}

} // namespace igl::vulkan
