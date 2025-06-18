/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFeatures.h>

namespace igl::vulkan {

VulkanFeatures::VulkanFeatures(VulkanContextConfig config) noexcept :
  // Vulkan 1.1
  vkPhysicalDeviceFeatures2_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .features =
          {
              .dualSrcBlend = config.enableDualSrcBlend ? VK_TRUE : VK_FALSE,
              .multiDrawIndirect = VK_TRUE,
              .drawIndirectFirstInstance = VK_TRUE,
              .depthBiasClamp = VK_TRUE,
#ifdef IGL_PLATFORM_ANDROID
              .fillModeNonSolid = VK_FALSE, // not well supported on Android
#else
              .fillModeNonSolid = VK_TRUE,
#endif
              .shaderInt16 = config.enableShaderInt16 ? VK_TRUE : VK_FALSE,
          },
  }),
  vkPhysicalDeviceSamplerYcbcrConversionFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
      .samplerYcbcrConversion = VK_TRUE,
  }),
  vkPhysicalDeviceShaderDrawParametersFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
      .shaderDrawParameters = config.enableShaderDrawParameters ? VK_TRUE : VK_FALSE,
  }),
  vkPhysicalDeviceMultiviewFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      .multiview = VK_TRUE,
      .multiviewGeometryShader = VK_FALSE,
      .multiviewTessellationShader = VK_FALSE,
  }),
  vkPhysicalDeviceBufferDeviceAddressFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
      .bufferDeviceAddress = VK_TRUE,
      .bufferDeviceAddressCaptureReplay = VK_FALSE,
      .bufferDeviceAddressMultiDevice = VK_FALSE,
  }),
  vkPhysicalDeviceDescriptorIndexingFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
      .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
      .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
      .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
      .shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
      .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
      .shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
      .shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
      .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
      .shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
      .shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
      .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
      .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
      .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
      .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
      .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
      .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
      .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
      .descriptorBindingPartiallyBound = VK_TRUE,
      .descriptorBindingVariableDescriptorCount = VK_FALSE,
      .runtimeDescriptorArray = VK_TRUE,
  }),
  vkPhysicalDevice16BitStorageFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
      .storageBuffer16BitAccess = config.enableStorageBuffer16BitAccess ? VK_TRUE : VK_FALSE,
      .uniformAndStorageBuffer16BitAccess = VK_FALSE,
      .storagePushConstant16 = VK_FALSE,
      .storageInputOutput16 = VK_FALSE,
  }),
  // Vulkan 1.2
  vkPhysicalDeviceShaderFloat16Int8Features_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
      .shaderFloat16 = VK_FALSE,
      .shaderInt8 = VK_FALSE,
  }),
  vkPhysicalDeviceIndexTypeUint8Features_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
      .indexTypeUint8 = VK_FALSE,
  }),
  vkPhysicalDeviceSynchronization2Features_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
      .synchronization2 = VK_TRUE,
  }),
  vkPhysicalDeviceTimelineSemaphoreFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
      .timelineSemaphore = VK_TRUE,
  }),
  vkPhysicalDeviceFragmentDensityMapFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
      .fragmentDensityMap = VK_TRUE,
  }),
  vkPhysicalDeviceVulkanMemoryModelFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR,
      .vulkanMemoryModel = VK_TRUE,
  }),
  vkPhysicalDeviceMultiviewPerViewViewportsFeatures_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM,
      .multiviewPerViewViewports = VK_TRUE,
  }),
  config_(config) {
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
  context.vf_.vkGetPhysicalDeviceFeatures2(physicalDevice, &vkPhysicalDeviceFeatures2_);
}

bool VulkanFeatures::hasExtension(const char* ext) const {
  for (const VkExtensionProperties& props : extensionProps_) {
    if (strcmp(ext, props.extensionName) == 0) {
      return true;
    }
  }
  return false;
}

Result VulkanFeatures::checkSelectedFeatures(
    const VulkanFeatures& availableFeatures) const noexcept {
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
  ENABLE_VULKAN_FEATURE(vkPhysicalDeviceFeatures2_.features,                        \
                        availableFeatureStruct.vkPhysicalDeviceFeatures2_.features, \
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
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           shaderSampledImageArrayNonUniformIndexing)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           descriptorBindingUniformBufferUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           descriptorBindingSampledImageUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           descriptorBindingStorageImageUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           descriptorBindingStorageBufferUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           descriptorBindingUpdateUnusedWhilePending)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           descriptorBindingPartiallyBound)
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceDescriptorIndexingFeatures_,
                           availableFeatures.vkPhysicalDeviceDescriptorIndexingFeatures_,
                           runtimeDescriptorArray)
  }
  ENABLE_FEATURE_1_1_EXT(vkPhysicalDevice16BitStorageFeatures_,
                         availableFeatures.vkPhysicalDevice16BitStorageFeatures_,
                         storageBuffer16BitAccess)
  if (has_VK_KHR_buffer_device_address) {
    ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceBufferDeviceAddressFeatures_,
                           availableFeatures.vkPhysicalDeviceBufferDeviceAddressFeatures_,
                           bufferDeviceAddress)
  }
  ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceMultiviewFeatures_,
                         availableFeatures.vkPhysicalDeviceMultiviewFeatures_,
                         multiview)
  ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceSamplerYcbcrConversionFeatures_,
                         availableFeatures.vkPhysicalDeviceSamplerYcbcrConversionFeatures_,
                         samplerYcbcrConversion)
  ENABLE_FEATURE_1_1_EXT(vkPhysicalDeviceShaderDrawParametersFeatures_,
                         availableFeatures.vkPhysicalDeviceShaderDrawParametersFeatures_,
                         shaderDrawParameters)
#undef ENABLE_FEATURE_1_1_EXT

#define ENABLE_FEATURE_1_2_EXT(requestedFeatureStruct, availableFeatureStruct, feature) \
  ENABLE_VULKAN_FEATURE(requestedFeatureStruct, availableFeatureStruct, feature, "1.2")
  ENABLE_FEATURE_1_2_EXT(vkPhysicalDeviceShaderFloat16Int8Features_,
                         availableFeatures.vkPhysicalDeviceShaderFloat16Int8Features_,
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
  vkPhysicalDeviceFeatures2_.pNext = nullptr;
  vkPhysicalDeviceSamplerYcbcrConversionFeatures_.pNext = nullptr;
  vkPhysicalDeviceShaderDrawParametersFeatures_.pNext = nullptr;
  vkPhysicalDeviceMultiviewFeatures_.pNext = nullptr;
  vkPhysicalDeviceIndexTypeUint8Features_.pNext = nullptr;
  vkPhysicalDeviceSynchronization2Features_.pNext = nullptr;
  vkPhysicalDeviceTimelineSemaphoreFeatures_.pNext = nullptr;
  vkPhysicalDeviceVulkanMemoryModelFeatures_.pNext = nullptr;
  vkPhysicalDeviceShaderFloat16Int8Features_.pNext = nullptr;
  vkPhysicalDevice16BitStorageFeatures_.pNext = nullptr;
  vkPhysicalDeviceBufferDeviceAddressFeatures_.pNext = nullptr;
  vkPhysicalDeviceDescriptorIndexingFeatures_.pNext = nullptr;
  vkPhysicalDeviceMultiviewPerViewViewportsFeatures_.pNext = nullptr;
  vkPhysicalDeviceFragmentDensityMapFeatures_.pNext = nullptr;

  // Add the required and optional features to the VkPhysicalDeviceFetaures2_
  ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceSamplerYcbcrConversionFeatures_);
  ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceShaderDrawParametersFeatures_);
  ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceMultiviewFeatures_);
  if (hasExtension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceShaderFloat16Int8Features_);
  }
  if (hasExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceBufferDeviceAddressFeatures_);
  }
  if (hasExtension(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceVulkanMemoryModelFeatures_);
  }
  if (hasExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceDescriptorIndexingFeatures_);
  }
  ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDevice16BitStorageFeatures_);

  if (hasExtension(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceIndexTypeUint8Features_);
  }
  if (hasExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceSynchronization2Features_);
  }
  if (hasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceTimelineSemaphoreFeatures_);
  }
  if (hasExtension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceFragmentDensityMapFeatures_);
  }
  if (config_.enableMultiviewPerViewViewports) {
    if (hasExtension(VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_EXTENSION_NAME)) {
      ivkAddNext(&vkPhysicalDeviceFeatures2_, &vkPhysicalDeviceMultiviewPerViewViewportsFeatures_);
    } else {
      IGL_LOG_ERROR("VK_QCOM_multiview_per_view_viewports extension not supported");
    }
  }
}

VulkanFeatures& VulkanFeatures::operator=(const VulkanFeatures& other) noexcept {
  if (this == &other) {
    return *this;
  }

  const bool sameConfiguration =
      config_.enableDescriptorIndexing == other.config_.enableDescriptorIndexing;
  if (!sameConfiguration) {
    return *this;
  }

  vkPhysicalDeviceFeatures2_ = other.vkPhysicalDeviceFeatures2_;

  vkPhysicalDeviceSamplerYcbcrConversionFeatures_ =
      other.vkPhysicalDeviceSamplerYcbcrConversionFeatures_;
  vkPhysicalDeviceShaderDrawParametersFeatures_ =
      other.vkPhysicalDeviceShaderDrawParametersFeatures_;
  vkPhysicalDeviceMultiviewFeatures_ = other.vkPhysicalDeviceMultiviewFeatures_;
  vkPhysicalDeviceBufferDeviceAddressFeatures_ = other.vkPhysicalDeviceBufferDeviceAddressFeatures_;
  vkPhysicalDeviceDescriptorIndexingFeatures_ = other.vkPhysicalDeviceDescriptorIndexingFeatures_;
  vkPhysicalDevice16BitStorageFeatures_ = other.vkPhysicalDevice16BitStorageFeatures_;

  // Vulkan 1.2
  vkPhysicalDeviceVulkanMemoryModelFeatures_ = other.vkPhysicalDeviceVulkanMemoryModelFeatures_;
  vkPhysicalDeviceShaderFloat16Int8Features_ = other.vkPhysicalDeviceShaderFloat16Int8Features_;
  vkPhysicalDeviceIndexTypeUint8Features_ = other.vkPhysicalDeviceIndexTypeUint8Features_;
  vkPhysicalDeviceSynchronization2Features_ = other.vkPhysicalDeviceSynchronization2Features_;
  vkPhysicalDeviceTimelineSemaphoreFeatures_ = other.vkPhysicalDeviceTimelineSemaphoreFeatures_;
  vkPhysicalDeviceFragmentDensityMapFeatures_ = other.vkPhysicalDeviceFragmentDensityMapFeatures_;
  vkPhysicalDeviceMultiviewPerViewViewportsFeatures_ =
      other.vkPhysicalDeviceMultiviewPerViewViewportsFeatures_;

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
#endif
  enable(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, ExtensionType::Device);
#if !IGL_PLATFORM_ANDROID || !IGL_DEBUG
  // On Android, vkEnumerateInstanceExtensionProperties crashes when validation layers are
  // enabled for DEBUG builds. https://issuetracker.google.com/issues/209835779?pli=1 Hence,
  // don't enable some extensions on Android which are not present and no way to check without
  // crashing.
  has_VK_KHR_shader_non_semantic_info =
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

  has_VK_KHR_vulkan_memory_model =
      enable(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, ExtensionType::Device);

  has_VK_EXT_descriptor_indexing =
      enable(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, ExtensionType::Device);

  has_VK_EXT_fragment_density_map =
      enable(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, ExtensionType::Device);

  if (config_.enableMultiviewPerViewViewports) {
    has_VK_QCOM_multiview_per_view_viewports =
        enable(VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_EXTENSION_NAME, ExtensionType::Device);
    IGL_SOFT_ASSERT(has_VK_QCOM_multiview_per_view_viewports,
                    "VK_QCOM_multiview_per_view_viewports is not supported");
  }
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
