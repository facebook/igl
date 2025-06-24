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
  vkPhysicalDeviceFeatures2({
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
  featuresSamplerYcbcrConversion({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
      .samplerYcbcrConversion = VK_TRUE,
  }),
  featuresShaderDrawParameters({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
      .shaderDrawParameters = config.enableShaderDrawParameters ? VK_TRUE : VK_FALSE,
  }),
  featuresMultiview({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      .multiview = VK_TRUE,
      .multiviewGeometryShader = VK_FALSE,
      .multiviewTessellationShader = VK_FALSE,
  }),
  featuresBufferDeviceAddress({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
      .bufferDeviceAddress = VK_TRUE,
      .bufferDeviceAddressCaptureReplay = VK_FALSE,
      .bufferDeviceAddressMultiDevice = VK_FALSE,
  }),
  featuresDescriptorIndexing({
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
  features16BitStorage({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
      .storageBuffer16BitAccess = config.enableStorageBuffer16BitAccess ? VK_TRUE : VK_FALSE,
      .uniformAndStorageBuffer16BitAccess = VK_FALSE,
      .storagePushConstant16 = VK_FALSE,
      .storageInputOutput16 = VK_FALSE,
  }),
  // Vulkan 1.2
  featuresShaderFloat16Int8({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
      .shaderFloat16 = VK_FALSE,
      .shaderInt8 = VK_FALSE,
  }),
  featuresIndexTypeUint8({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
      .indexTypeUint8 = VK_FALSE,
  }),
  featuresSynchronization2({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
      .synchronization2 = VK_TRUE,
  }),
  featuresTimelineSemaphore({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
      .timelineSemaphore = VK_TRUE,
  }),
  featuresFragmentDensityMap({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
      .fragmentDensityMap = VK_TRUE,
  }),
  featuresVulkanMemoryModel({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR,
      .vulkanMemoryModel = VK_TRUE,
  }),
  features8BitStorage({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
      .storageBuffer8BitAccess = VK_TRUE,
      .uniformAndStorageBuffer8BitAccess = VK_FALSE,
      .storagePushConstant8 = VK_FALSE,
  }),
  featuresUniformBufferStandardLayout({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR,
      .uniformBufferStandardLayout = VK_TRUE,
  }),
  featuresMultiviewPerViewViewports({
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
  context.vf_.vkGetPhysicalDeviceFeatures2(physicalDevice, &vkPhysicalDeviceFeatures2);
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

#define ENABLE_FEATURE_1_1(availableFeatureStruct, feature)                        \
  ENABLE_VULKAN_FEATURE(vkPhysicalDeviceFeatures2.features,                        \
                        availableFeatureStruct.vkPhysicalDeviceFeatures2.features, \
                        feature,                                                   \
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
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           shaderSampledImageArrayNonUniformIndexing)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           descriptorBindingUniformBufferUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           descriptorBindingSampledImageUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           descriptorBindingStorageImageUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           descriptorBindingStorageBufferUpdateAfterBind)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           descriptorBindingUpdateUnusedWhilePending)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           descriptorBindingPartiallyBound)
    ENABLE_FEATURE_1_1_EXT(featuresDescriptorIndexing,
                           availableFeatures.featuresDescriptorIndexing,
                           runtimeDescriptorArray)
  }
  ENABLE_FEATURE_1_1_EXT(
      features16BitStorage, availableFeatures.features16BitStorage, storageBuffer16BitAccess)
  if (has_VK_KHR_buffer_device_address) {
    ENABLE_FEATURE_1_1_EXT(featuresBufferDeviceAddress,
                           availableFeatures.featuresBufferDeviceAddress,
                           bufferDeviceAddress)
  }
  ENABLE_FEATURE_1_1_EXT(featuresMultiview, availableFeatures.featuresMultiview, multiview)
  ENABLE_FEATURE_1_1_EXT(featuresSamplerYcbcrConversion,
                         availableFeatures.featuresSamplerYcbcrConversion,
                         samplerYcbcrConversion)
  ENABLE_FEATURE_1_1_EXT(featuresShaderDrawParameters,
                         availableFeatures.featuresShaderDrawParameters,
                         shaderDrawParameters)
#undef ENABLE_FEATURE_1_1_EXT

#define ENABLE_FEATURE_1_2_EXT(requestedFeatureStruct, availableFeatureStruct, feature) \
  ENABLE_VULKAN_FEATURE(requestedFeatureStruct, availableFeatureStruct, feature, "1.2")
  ENABLE_FEATURE_1_2_EXT(
      featuresShaderFloat16Int8, availableFeatures.featuresShaderFloat16Int8, shaderFloat16)
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
  vkPhysicalDeviceFeatures2.pNext = nullptr;
  featuresSamplerYcbcrConversion.pNext = nullptr;
  featuresShaderDrawParameters.pNext = nullptr;
  featuresMultiview.pNext = nullptr;
  featuresIndexTypeUint8.pNext = nullptr;
  featuresSynchronization2.pNext = nullptr;
  featuresTimelineSemaphore.pNext = nullptr;
  featuresVulkanMemoryModel.pNext = nullptr;
  featuresShaderFloat16Int8.pNext = nullptr;
  features16BitStorage.pNext = nullptr;
  featuresBufferDeviceAddress.pNext = nullptr;
  featuresDescriptorIndexing.pNext = nullptr;
  featuresMultiviewPerViewViewports.pNext = nullptr;
  featuresFragmentDensityMap.pNext = nullptr;
  features8BitStorage.pNext = nullptr;
  featuresUniformBufferStandardLayout.pNext = nullptr;

  // Add the required and optional features to the VkPhysicalDeviceFetaures2_
  ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresSamplerYcbcrConversion);
  ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresShaderDrawParameters);
  ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresMultiview);
  if (hasExtension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresShaderFloat16Int8);
  }
  if (hasExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresBufferDeviceAddress);
  }
  if (hasExtension(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresVulkanMemoryModel);
  }
  if (hasExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresDescriptorIndexing);
  }
  ivkAddNext(&vkPhysicalDeviceFeatures2, &features16BitStorage);

  if (hasExtension(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresIndexTypeUint8);
  }
  if (hasExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresSynchronization2);
  }
  if (hasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresTimelineSemaphore);
  }
  if (hasExtension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresFragmentDensityMap);
  }
  if (hasExtension(VK_KHR_8BIT_STORAGE_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &features8BitStorage);
  }
  if (hasExtension(VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME)) {
    ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresUniformBufferStandardLayout);
  }
  if (config_.enableMultiviewPerViewViewports) {
    if (hasExtension(VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_EXTENSION_NAME)) {
      ivkAddNext(&vkPhysicalDeviceFeatures2, &featuresMultiviewPerViewViewports);
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

  vkPhysicalDeviceFeatures2 = other.vkPhysicalDeviceFeatures2;

  featuresSamplerYcbcrConversion = other.featuresSamplerYcbcrConversion;
  featuresShaderDrawParameters = other.featuresShaderDrawParameters;
  featuresMultiview = other.featuresMultiview;
  featuresBufferDeviceAddress = other.featuresBufferDeviceAddress;
  featuresDescriptorIndexing = other.featuresDescriptorIndexing;
  features16BitStorage = other.features16BitStorage;

  // Vulkan 1.2
  featuresVulkanMemoryModel = other.featuresVulkanMemoryModel;
  featuresShaderFloat16Int8 = other.featuresShaderFloat16Int8;
  featuresIndexTypeUint8 = other.featuresIndexTypeUint8;
  featuresSynchronization2 = other.featuresSynchronization2;
  featuresTimelineSemaphore = other.featuresTimelineSemaphore;
  featuresFragmentDensityMap = other.featuresFragmentDensityMap;
  features8BitStorage = other.features8BitStorage;
  featuresUniformBufferStandardLayout = other.featuresUniformBufferStandardLayout;
  featuresMultiviewPerViewViewports = other.featuresMultiviewPerViewViewports;

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
#endif // IGL_PLATFORM_MACOSX

#if !IGL_PLATFORM_ANDROID
  if (config.enableValidation) {
    enable(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, ExtensionType::Instance);
  }
#endif // !IGL_PLATFORM_ANDROID

  has_VK_EXT_headless_surface =
      enable(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, ExtensionType::Instance);

  if (config.headless) {
    if (!has_VK_EXT_headless_surface) {
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
  enable(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_KHR_SWAPCHAIN_EXTENSION_NAME, ExtensionType::Device);

#if IGL_PLATFORM_ANDROID
  enable(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, ExtensionType::Device);
  enable(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME, ExtensionType::Device);
#endif // IGL_PLATFORM_ANDROID

#if !IGL_DEBUG
  has_VK_KHR_shader_non_semantic_info =
      enable(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, ExtensionType::Device);
#endif // !IGL_DEBUG

#if IGL_PLATFORM_MACOSX
  std::ignore = IGL_DEBUG_VERIFY(enable("VK_KHR_portability_subset", ExtensionType::Device));
#endif // IGL_PLATFORM_MACOSX

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
  has_VK_EXT_queue_family_foreign =
      enable(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_timeline_semaphore =
      enable(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_uniform_buffer_standard_layout =
      enable(VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_synchronization2 =
      enable(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_8bit_storage = enable(VK_KHR_8BIT_STORAGE_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_buffer_device_address =
      enable(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, ExtensionType::Device);

  has_VK_KHR_create_renderpass2 =
      enable(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, ExtensionType::Device);

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
