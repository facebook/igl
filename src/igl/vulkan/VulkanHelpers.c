/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "VulkanHelpers.h"

#include <assert.h>

static const char* kDefaultValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

const char* ivkGetVulkanResultString(VkResult result) {
#define RESULT_CASE(res) \
  case res:              \
    return #res
  switch (result) {
    RESULT_CASE(VK_SUCCESS);
    RESULT_CASE(VK_NOT_READY);
    RESULT_CASE(VK_TIMEOUT);
    RESULT_CASE(VK_EVENT_SET);
    RESULT_CASE(VK_EVENT_RESET);
    RESULT_CASE(VK_INCOMPLETE);
    RESULT_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
    RESULT_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    RESULT_CASE(VK_ERROR_INITIALIZATION_FAILED);
    RESULT_CASE(VK_ERROR_DEVICE_LOST);
    RESULT_CASE(VK_ERROR_MEMORY_MAP_FAILED);
    RESULT_CASE(VK_ERROR_LAYER_NOT_PRESENT);
    RESULT_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
    RESULT_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
    RESULT_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
    RESULT_CASE(VK_ERROR_TOO_MANY_OBJECTS);
    RESULT_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
    RESULT_CASE(VK_ERROR_SURFACE_LOST_KHR);
    RESULT_CASE(VK_ERROR_OUT_OF_DATE_KHR);
    RESULT_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    RESULT_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    RESULT_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
    RESULT_CASE(VK_ERROR_FRAGMENTED_POOL);
    RESULT_CASE(VK_ERROR_UNKNOWN);
    // Provided by VK_VERSION_1_1
    RESULT_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
    // Provided by VK_VERSION_1_1
    RESULT_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
    // Provided by VK_VERSION_1_2
    RESULT_CASE(VK_ERROR_FRAGMENTATION);
    // Provided by VK_VERSION_1_2
    RESULT_CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    // Provided by VK_KHR_swapchain
    RESULT_CASE(VK_SUBOPTIMAL_KHR);
    // Provided by VK_NV_glsl_shader
    RESULT_CASE(VK_ERROR_INVALID_SHADER_NV);
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    // Provided by VK_KHR_video_queue
    RESULT_CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
#endif
    // Provided by VK_EXT_image_drm_format_modifier
    RESULT_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    // Provided by VK_KHR_global_priority
    RESULT_CASE(VK_ERROR_NOT_PERMITTED_KHR);
    // Provided by VK_EXT_full_screen_exclusive
    RESULT_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_THREAD_IDLE_KHR);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_THREAD_DONE_KHR);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_OPERATION_DEFERRED_KHR);
    // Provided by VK_KHR_deferred_host_operations
    RESULT_CASE(VK_OPERATION_NOT_DEFERRED_KHR);
  default:
    return "Unknown VkResult Value";
  }
#undef RESULT_CASE
}

VkResult ivkCreateInstance(const struct VulkanFunctionTable* vt,
                           uint32_t apiVersion,
                           uint32_t enableValidation,
                           uint32_t enableGPUAssistedValidation,
                           uint32_t enableSynchronizationValidation,
                           size_t numExtensions,
                           const char** extensions,
                           VkInstance* outInstance) {
  // Validation Features not available on most Android devices
#if !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACOS
  VkValidationFeatureEnableEXT validationFeaturesEnabled[2];
  int validationFeaturesCount = 0;
  if (enableGPUAssistedValidation) {
    validationFeaturesEnabled[validationFeaturesCount++] =
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT;
  }
  if (enableSynchronizationValidation) {
    validationFeaturesEnabled[validationFeaturesCount++] =
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
  }

  const VkValidationFeaturesEXT features = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .pNext = NULL,
      .enabledValidationFeatureCount = validationFeaturesCount,
      .pEnabledValidationFeatures = validationFeaturesCount > 0 ? validationFeaturesEnabled : NULL,
  };
#endif // !IGL_PLATFORM_ANDROID

  const VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = NULL,
      .pApplicationName = "IGL/Vulkan",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "IGL/Vulkan",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = apiVersion,
  };

  const VkInstanceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#if !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACOS
      .pNext = enableValidation ? &features : NULL,
#endif
      .pApplicationInfo = &appInfo,
#if !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACOS
      .enabledLayerCount = enableValidation ? IGL_ARRAY_NUM_ELEMENTS(kDefaultValidationLayers) : 0,
      .ppEnabledLayerNames = enableValidation ? kDefaultValidationLayers : NULL,
#endif
      .enabledExtensionCount = (uint32_t)numExtensions,
      .ppEnabledExtensionNames = extensions,
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
  };
  (void)kDefaultValidationLayers; // maybe unused

  return vt->vkCreateInstance(&ci, NULL, outInstance);
}

VkResult ivkCreateCommandPool(const struct VulkanFunctionTable* vt,
                              VkDevice device,
                              VkCommandPoolCreateFlags flags,
                              uint32_t queueFamilyIndex,
                              VkCommandPool* outCommandPool) {
  const VkCommandPoolCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = flags,
      .queueFamilyIndex = queueFamilyIndex,
  };

  return vt->vkCreateCommandPool(device, &ci, NULL, outCommandPool);
}

VkResult ivkAllocateCommandBuffer(const struct VulkanFunctionTable* vt,
                                  VkDevice device,
                                  VkCommandPool commandPool,
                                  VkCommandBuffer* outCommandBuffer) {
  const VkCommandBufferAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = NULL,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  return vt->vkAllocateCommandBuffers(device, &ai, outCommandBuffer);
}

VkResult ivkAllocateMemory(const struct VulkanFunctionTable* vt,
                           VkPhysicalDevice physDev,
                           VkDevice device,
                           const VkMemoryRequirements* memRequirements,
                           VkMemoryPropertyFlags props,
                           bool enableBufferDeviceAddress,
                           VkDeviceMemory* outMemory) {
  assert(memRequirements);

  const VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = enableBufferDeviceAddress ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0,
  };

  const VkMemoryAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = memRequirements->size,
      .memoryTypeIndex = ivkFindMemoryType(vt, physDev, memRequirements->memoryTypeBits, props),
  };

  return vt->vkAllocateMemory(device, &ai, NULL, outMemory);
}

VkResult ivkAllocateMemory2(const struct VulkanFunctionTable* vt,
                            VkPhysicalDevice physDev,
                            VkDevice device,
                            const VkMemoryRequirements2* memRequirements,
                            VkMemoryPropertyFlags props,
                            bool enableBufferDeviceAddress,
                            VkDeviceMemory* outMemory) {
  assert(memRequirements);

  const VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = enableBufferDeviceAddress ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0,
  };

  const VkMemoryAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = memRequirements->memoryRequirements.size,
      .memoryTypeIndex =
          ivkFindMemoryType(vt, physDev, memRequirements->memoryRequirements.memoryTypeBits, props),
  };

  return vt->vkAllocateMemory(device, &ai, NULL, outMemory);
}

VkImagePlaneMemoryRequirementsInfo ivkGetImagePlaneMemoryRequirementsInfo(
    VkImageAspectFlagBits plane) {
  const VkImagePlaneMemoryRequirementsInfo info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
      .pNext = NULL,
      .planeAspect = plane,
  };
  return info;
}

VkImageMemoryRequirementsInfo2 ivkGetImageMemoryRequirementsInfo2(
    const VkImagePlaneMemoryRequirementsInfo* next,
    VkImage image) {
  const VkImageMemoryRequirementsInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .pNext = next,
      .image = image,
  };
  return info;
}

VkBindImageMemoryInfo ivkGetBindImageMemoryInfo(const VkBindImagePlaneMemoryInfo* next,
                                                VkImage image,
                                                VkDeviceMemory memory) {
  const VkBindImageMemoryInfo info = {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .pNext = next,
      .image = image,
      .memory = memory,
      .memoryOffset = 0,
  };
  return info;
}

bool ivkIsHostVisibleSingleHeapMemory(const struct VulkanFunctionTable* vt,
                                      VkPhysicalDevice physDev) {
  VkPhysicalDeviceMemoryProperties memProperties;

  vt->vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

  if (memProperties.memoryHeapCount != 1) {
    return false;
  }

  const uint32_t flag = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memProperties.memoryTypes[i].propertyFlags & flag) == flag) {
      return true;
    }
  }

  return false;
}

uint32_t ivkFindMemoryType(const struct VulkanFunctionTable* vt,
                           VkPhysicalDevice physDev,
                           uint32_t memoryTypeBits,
                           VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vt->vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    const bool hasProperties = (memProperties.memoryTypes[i].propertyFlags & flags) == flags;
    if ((memoryTypeBits & (1 << i)) && hasProperties) {
      return i;
    }
  }

  assert(false);

  return 0;
}

VkResult ivkCreateSemaphore(const struct VulkanFunctionTable* vt,
                            VkDevice device,
                            bool exportable,
                            VkSemaphore* outSemaphore) {
  const VkExportSemaphoreCreateInfo exportInfo = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .pNext = NULL,
      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
  };

  const VkSemaphoreCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = exportable ? &exportInfo : NULL,
      .flags = 0,
  };
  return vt->vkCreateSemaphore(device, &ci, NULL, outSemaphore);
}

void ivkAddNext(void* node, const void* next) {
  if (!node || !next) {
    return;
  }

  VkBaseInStructure* cur = (VkBaseInStructure*)node;

  while (cur->pNext) {
    cur = (VkBaseInStructure*)cur->pNext;
  }

  cur->pNext = next;
}

VkResult ivkCreateDevice(const struct VulkanFunctionTable* vt,
                         VkPhysicalDevice physicalDevice,
                         size_t numQueueCreateInfos,
                         const VkDeviceQueueCreateInfo* queueCreateInfos,
                         size_t numDeviceExtensions,
                         const char** deviceExtensions,
                         const VkPhysicalDeviceFeatures2* supported,
                         VkDevice* outDevice) {
  assert(numQueueCreateInfos >= 1);
  const VkPhysicalDeviceFeatures deviceFeatures10 = {
      .dualSrcBlend = supported ? supported->features.dualSrcBlend : VK_TRUE,
      .multiDrawIndirect = supported ? supported->features.multiDrawIndirect : VK_TRUE,
      .drawIndirectFirstInstance = supported ? supported->features.drawIndirectFirstInstance
                                             : VK_TRUE,
      .depthBiasClamp = supported ? supported->features.depthBiasClamp : VK_TRUE,
      .fillModeNonSolid = supported ? supported->features.fillModeNonSolid : VK_TRUE,
      .shaderInt16 = supported ? supported->features.shaderInt16 : VK_TRUE,
  };
  VkDeviceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = (uint32_t)numQueueCreateInfos,
      .pQueueCreateInfos = queueCreateInfos,
      .enabledExtensionCount = (uint32_t)numDeviceExtensions,
      .ppEnabledExtensionNames = deviceExtensions,
      .pEnabledFeatures = &deviceFeatures10,
  };

  // Append all feature structs being requested for this device
  ci.pNext = supported->pNext;

  return vt->vkCreateDevice(physicalDevice, &ci, NULL, outDevice);
}

#if defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACCATALYST
#define VK_EXT_DEBUG_UTILS_SUPPORTED 1
#else
#define VK_EXT_DEBUG_UTILS_SUPPORTED 0
#endif

#if VK_EXT_DEBUG_UTILS_SUPPORTED
VkResult ivkCreateDebugUtilsMessenger(const struct VulkanFunctionTable* vt,
                                      VkInstance instance,
                                      PFN_vkDebugUtilsMessengerCallbackEXT callback,
                                      void* logUserData,
                                      VkDebugUtilsMessengerEXT* outMessenger) {
  // Some Android devices don't have the VK_EXT_debug_utils functions available, even though the
  // extension is supported and has been enabled
  if (vt->vkCreateDebugUtilsMessengerEXT == NULL) {
    return VK_SUCCESS;
  }

  const VkDebugUtilsMessengerCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = callback,
      .pUserData = logUserData,
  };

  return vt->vkCreateDebugUtilsMessengerEXT(instance, &ci, NULL, outMessenger);
}
#else // VK_EXT_DEBUG_UTILS_SUPPORTED
// Stub version
VkResult ivkCreateDebugUtilsMessenger(const struct VulkanFunctionTable* vt,
                                      VkInstance instance,
                                      PFN_vkDebugUtilsMessengerCallbackEXT callback,
                                      void* logUserData,
                                      VkDebugUtilsMessengerEXT* outMessenger) {
  return VK_SUCCESS;
}
#endif // VK_EXT_DEBUG_UTILS_SUPPORTED

#if defined(VK_EXT_debug_report)
VkResult ivkCreateDebugReportMessenger(const struct VulkanFunctionTable* vt,
                                       VkInstance instance,
                                       PFN_vkDebugReportCallbackEXT callback,
                                       void* logUserData,
                                       VkDebugReportCallbackEXT* outMessenger) {
  VkDebugReportCallbackCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
      .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
               VK_DEBUG_REPORT_DEBUG_BIT_EXT,
      .pfnCallback = callback,
      .pUserData = logUserData,
  };

  return vt->vkCreateDebugReportCallbackEXT(instance, &ci, NULL, outMessenger);
}
#else // defined(VK_EXT_debug_report)
// Stub version
VkResult ivkCreateDebugReportMessenger(VkInstance instance,
                                       PFN_vkDebugReportCallbackEXT callback,
                                       void* logUserData,
                                       VkDebugReportCallbackEXT* outMessenger) {
  return VK_SUCCESS;
}
#endif // defined(VK_EXT_debug_report)

VkResult ivkCreateSurface(const struct VulkanFunctionTable* vt,
                          VkInstance instance,
                          void* window,
                          void* display,
                          void* layer,
                          VkSurfaceKHR* outSurface) {
#if !defined(VK_USE_PLATFORM_METAL_EXT)
  (void)layer;
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  const VkWin32SurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandle(NULL),
      .hwnd = (HWND)window,
  };
  return vkCreateWin32SurfaceKHR(instance, &ci, NULL, outSurface);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
  const VkAndroidSurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .window = window,
  };
  return vt->vkCreateAndroidSurfaceKHR(instance, &ci, NULL, outSurface);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
  const VkMetalSurfaceCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
      .pNext = NULL,
      .flags = 0,
      .pLayer = layer,
  };
  return vt->vkCreateMetalSurfaceEXT(instance, &ci, NULL, outSurface);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  const VkXlibSurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .dpy = (Display*)display,
      .window = (Window)window,
  };
  return vt->vkCreateXlibSurfaceKHR(instance, &ci, NULL, outSurface);
#else
  (void)instance;
  (void)window;
  (void)outSurface;
  // TODO: implement for other platforms
  return VK_NOT_READY;
#endif
}

VkResult ivkCreateSwapchain(const struct VulkanFunctionTable* vt,
                            VkDevice device,
                            VkSurfaceKHR surface,
                            uint32_t minImageCount,
                            VkSurfaceFormatKHR surfaceFormat,
                            VkPresentModeKHR presentMode,
                            const VkSurfaceCapabilitiesKHR* caps,
                            VkImageUsageFlags imageUsage,
                            uint32_t queueFamilyIndex,
                            uint32_t width,
                            uint32_t height,
                            VkSwapchainKHR* outSwapchain) {
  assert(caps);
  const bool isCompositeAlphaOpaqueSupported =
      (caps->supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0;
  const VkSwapchainCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = minImageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = {.width = width, .height = height},
      .imageArrayLayers = 1,
      .imageUsage = imageUsage,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex,
      .preTransform = caps->currentTransform,
      .compositeAlpha = isCompositeAlphaOpaqueSupported ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                                                        : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };
  return vt->vkCreateSwapchainKHR(device, &ci, NULL, outSwapchain);
}

VkResult ivkCreateHeadlessSurface(const struct VulkanFunctionTable* vt,
                                  VkInstance instance,
                                  VkSurfaceKHR* outSurface) {
  const VkHeadlessSurfaceCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
      .pNext = NULL,
      .flags = 0,
  };

  return vt->vkCreateHeadlessSurfaceEXT(instance, &ci, NULL, outSurface);
}

VkSamplerCreateInfo ivkGetSamplerCreateInfo(VkFilter minFilter,
                                            VkFilter magFilter,
                                            VkSamplerMipmapMode mipmapMode,
                                            VkSamplerAddressMode addressModeU,
                                            VkSamplerAddressMode addressModeV,
                                            VkSamplerAddressMode addressModeW,
                                            float minLod,
                                            float maxLod) {
  const VkSamplerCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .magFilter = magFilter,
      .minFilter = minFilter,
      .mipmapMode = mipmapMode,
      .addressModeU = addressModeU,
      .addressModeV = addressModeV,
      .addressModeW = addressModeW,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0f,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = minLod,
      .maxLod = maxLod,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };
  return ci;
}

VkSamplerYcbcrConversionCreateInfo ivkGetSamplerYcbcrCreateInfo(VkFormat format) {
  const VkSamplerYcbcrConversionCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
      .pNext = NULL,
      .format = format,
      .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
      .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
      .components =
          {
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT,
      .yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT,
      .chromaFilter = VK_FILTER_LINEAR,
      .forceExplicitReconstruction = VK_FALSE,
  };
  return ci;
}

VkImageViewCreateInfo ivkGetImageViewCreateInfo(VkImage image,
                                                VkImageViewType type,
                                                VkFormat imageFormat,
                                                VkImageSubresourceRange range) {
  const VkImageViewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = type,
      .format = imageFormat,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = range,
  };
  return ci;
}

VkResult ivkCreateFramebuffer(const struct VulkanFunctionTable* vt,
                              VkDevice device,
                              uint32_t width,
                              uint32_t height,
                              VkRenderPass renderPass,
                              size_t numAttachments,
                              const VkImageView* attachments,
                              VkFramebuffer* outFramebuffer) {
  const VkFramebufferCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = renderPass,
      .attachmentCount = (uint32_t)numAttachments,
      .pAttachments = attachments,
      .width = width,
      .height = height,
      .layers = 1,
  };
  return vt->vkCreateFramebuffer(device, &ci, NULL, outFramebuffer);
}

VkAttachmentDescription2 ivkGetAttachmentDescriptionColor(VkFormat format,
                                                          VkAttachmentLoadOp loadOp,
                                                          VkAttachmentStoreOp storeOp,
                                                          VkImageLayout initialLayout,
                                                          VkImageLayout finalLayout) {
  const VkAttachmentDescription2 desc = {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  };
  return desc;
}

VkAttachmentReference2 ivkGetAttachmentReferenceColor(uint32_t idx) {
  const VkAttachmentReference2 ref = {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .attachment = idx,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
  };
  return ref;
}

VkResult ivkCreateRenderPass(const struct VulkanFunctionTable* vt,
                             VkDevice device,
                             uint32_t numAttachments,
                             const VkAttachmentDescription* attachments,
                             const VkSubpassDescription* subpass,
                             const VkSubpassDependency* dependency,
                             const VkRenderPassMultiviewCreateInfo* renderPassMultiview,
                             VkRenderPass* outRenderPass) {
  const VkRenderPassCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = renderPassMultiview,
      .attachmentCount = numAttachments,
      .pAttachments = attachments,
      .subpassCount = 1,
      .pSubpasses = subpass,
      .dependencyCount = 1,
      .pDependencies = dependency,
  };
  return vt->vkCreateRenderPass(device, &ci, NULL, outRenderPass);
}

VkDescriptorSetLayoutBinding ivkGetDescriptorSetLayoutBinding(uint32_t binding,
                                                              VkDescriptorType descriptorType,
                                                              uint32_t descriptorCount,
                                                              VkShaderStageFlags stageFlags) {
  const VkDescriptorSetLayoutBinding bind = {
      .binding = binding,
      .descriptorType = descriptorType,
      .descriptorCount = descriptorCount,
      .stageFlags = stageFlags,
      .pImmutableSamplers = NULL,
  };
  return bind;
}

VkAttachmentDescription ivkGetAttachmentDescription(VkFormat format,
                                                    VkAttachmentLoadOp loadOp,
                                                    VkAttachmentStoreOp storeOp,
                                                    VkImageLayout initialLayout,
                                                    VkImageLayout finalLayout,
                                                    VkSampleCountFlagBits samples) {
  const VkAttachmentDescription desc = {
      .flags = 0,
      .format = format,
      .samples = samples,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  };
  return desc;
}

VkAttachmentReference ivkGetAttachmentReference(uint32_t attachment, VkImageLayout layout) {
  const VkAttachmentReference ref = {
      .attachment = attachment,
      .layout = layout,
  };
  return ref;
}

VkSubpassDescription ivkGetSubpassDescription(uint32_t numColorAttachments,
                                              const VkAttachmentReference* refsColor,
                                              const VkAttachmentReference* refsColorResolve,
                                              const VkAttachmentReference* refDepth) {
  const VkSubpassDescription desc = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = numColorAttachments,
      .pColorAttachments = refsColor,
      .pResolveAttachments = refsColorResolve,
      .pDepthStencilAttachment = refDepth,
  };
  return desc;
}

VkSubpassDependency ivkGetSubpassDependency(void) {
  const VkSubpassDependency dep = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
  };
  return dep;
}

VkRenderPassMultiviewCreateInfo ivkGetRenderPassMultiviewCreateInfo(
    const uint32_t* viewMask,
    const uint32_t* correlationMask) {
  const VkRenderPassMultiviewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
      .subpassCount = 1,
      .pViewMasks = viewMask,
      .correlationMaskCount = 1,
      .pCorrelationMasks = correlationMask,
  };

  return ci;
}

VkResult ivkCreateDescriptorSetLayout(const struct VulkanFunctionTable* vt,
                                      VkDevice device,
                                      VkDescriptorSetLayoutCreateFlags flags,
                                      uint32_t numBindings,
                                      const VkDescriptorSetLayoutBinding* bindings,
                                      const VkDescriptorBindingFlags* bindingFlags,
                                      VkDescriptorSetLayout* outLayout) {
  // Do not enable VK_EXT_descriptor_indexing for Android until
  // we fix the extension enumeration crash when the validation layer is enabled
#if !IGL_PLATFORM_ANDROID
  const VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlagsCI = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
      .bindingCount = numBindings,
      .pBindingFlags = bindingFlags,
  };
#endif // !IGL_PLATFORM_ANDROID

  const VkDescriptorSetLayoutCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
#if !IGL_PLATFORM_ANDROID
      .pNext = &setLayoutBindingFlagsCI,
      .flags = flags,
#endif
      .bindingCount = numBindings,
      .pBindings = bindings,
  };
  return vt->vkCreateDescriptorSetLayout(device, &ci, NULL, outLayout);
}

VkResult ivkAllocateDescriptorSet(const struct VulkanFunctionTable* vt,
                                  VkDevice device,
                                  VkDescriptorPool pool,
                                  VkDescriptorSetLayout layout,
                                  VkDescriptorSet* outDescriptorSet) {
  const VkDescriptorSetAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout,
  };
  return vt->vkAllocateDescriptorSets(device, &ai, outDescriptorSet);
}

VkResult ivkCreateDescriptorPool(const struct VulkanFunctionTable* vt,
                                 VkDevice device,
                                 VkDescriptorPoolCreateFlags flags,
                                 uint32_t maxDescriptorSets,
                                 uint32_t numPoolSizes,
                                 const VkDescriptorPoolSize* poolSizes,
                                 VkDescriptorPool* outDescriptorPool) {
  const VkDescriptorPoolCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = flags,
      .maxSets = maxDescriptorSets,
      .poolSizeCount = numPoolSizes,
      .pPoolSizes = poolSizes,
  };
  return vt->vkCreateDescriptorPool(device, &ci, NULL, outDescriptorPool);
}

VkResult ivkBeginCommandBuffer(const struct VulkanFunctionTable* vt, VkCommandBuffer buffer) {
  const VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  return vt->vkBeginCommandBuffer(buffer, &bi);
}

VkResult ivkEndCommandBuffer(const struct VulkanFunctionTable* vt, VkCommandBuffer buffer) {
  return vt->vkEndCommandBuffer(buffer);
}

VkSubmitInfo ivkGetSubmitInfo(const VkCommandBuffer* buffer,
                              uint32_t numWaitSemaphores,
                              const VkSemaphore* waitSemaphores,
                              const VkPipelineStageFlags* waitStageMasks,
                              const VkSemaphore* releaseSemaphore) {
  const VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = numWaitSemaphores,
      .pWaitSemaphores = numWaitSemaphores ? waitSemaphores : NULL,
      .pWaitDstStageMask = waitStageMasks,
      .commandBufferCount = 1,
      .pCommandBuffers = buffer,
      .signalSemaphoreCount = releaseSemaphore ? 1 : 0,
      .pSignalSemaphores = releaseSemaphore,
  };
  return si;
}

VkClearValue ivkGetClearColorValue(float r, float g, float b, float a) {
  const VkClearValue value = {
      .color = {.float32 = {r, g, b, a}},
  };
  return value;
}

VkClearValue ivkGetClearDepthStencilValue(float depth, uint32_t stencil) {
  const VkClearValue value = {
      .depthStencil = {.depth = depth, .stencil = stencil},
  };
  return value;
}

VkBufferCreateInfo ivkGetBufferCreateInfo(uint64_t size, VkBufferUsageFlags usage) {
  const VkBufferCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
  };
  return ci;
}

VkImageCreateInfo ivkGetImageCreateInfo(VkImageType type,
                                        VkFormat imageFormat,
                                        VkImageTiling tiling,
                                        VkImageUsageFlags usage,
                                        VkExtent3D extent,
                                        uint32_t mipLevels,
                                        uint32_t arrayLayers,
                                        VkImageCreateFlags flags,
                                        VkSampleCountFlags samples) {
  const VkImageCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = flags,
      .imageType = type,
      .format = imageFormat,
      .extent = extent,
      .mipLevels = mipLevels,
      .arrayLayers = arrayLayers,
      .samples = samples,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  return ci;
}

VkPipelineVertexInputStateCreateInfo ivkGetPipelineVertexInputStateCreateInfo_Empty(void) {
  return ivkGetPipelineVertexInputStateCreateInfo(0, NULL, 0, NULL);
}

VkPipelineVertexInputStateCreateInfo ivkGetPipelineVertexInputStateCreateInfo(
    uint32_t vbCount,
    const VkVertexInputBindingDescription* bindings,
    uint32_t vaCount,
    const VkVertexInputAttributeDescription* attributes) {
  const VkPipelineVertexInputStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = vbCount,
      .pVertexBindingDescriptions = bindings,
      .vertexAttributeDescriptionCount = vaCount,
      .pVertexAttributeDescriptions = attributes};
  return ci;
}

VkPipelineInputAssemblyStateCreateInfo ivkGetPipelineInputAssemblyStateCreateInfo(
    VkPrimitiveTopology topology,
    VkBool32 enablePrimitiveRestart) {
  const VkPipelineInputAssemblyStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .flags = 0,
      .topology = topology,
      .primitiveRestartEnable = enablePrimitiveRestart,
  };
  return ci;
}

VkPipelineDynamicStateCreateInfo ivkGetPipelineDynamicStateCreateInfo(
    uint32_t numDynamicStates,
    const VkDynamicState* dynamicStates) {
  const VkPipelineDynamicStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = numDynamicStates,
      .pDynamicStates = dynamicStates,
  };
  return ci;
}

VkPipelineViewportStateCreateInfo ivkGetPipelineViewportStateCreateInfo(const VkViewport* viewport,
                                                                        const VkRect2D* scissor) {
  // viewport and scissor can be NULL if the viewport state is dynamic
  // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPipelineViewportStateCreateInfo.html
  const VkPipelineViewportStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = viewport,
      .scissorCount = 1,
      .pScissors = scissor,
  };
  return ci;
}

VkPipelineRasterizationStateCreateInfo ivkGetPipelineRasterizationStateCreateInfo(
    VkPolygonMode polygonMode,
    VkCullModeFlags cullModeFlags) {
  const VkPipelineRasterizationStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = polygonMode,
      .cullMode = cullModeFlags,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };
  return ci;
}

VkPipelineMultisampleStateCreateInfo ivkGetPipelineMultisampleStateCreateInfo_Empty(void) {
  const VkPipelineMultisampleStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = NULL,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = NULL,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };
  return ci;
}

VkPipelineDepthStencilStateCreateInfo ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests(
    void) {
  const VkPipelineDepthStencilStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_NEVER,
              .compareMask = 0,
              .writeMask = 0,
              .reference = 0,
          },
      .back =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_NEVER,
              .compareMask = 0,
              .writeMask = 0,
              .reference = 0,
          },
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };
  return ci;
}

VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState_NoBlending(void) {
  const VkPipelineColorBlendAttachmentState state = {
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  return state;
}

VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState(
    bool blendEnable,
    VkBlendFactor srcColorBlendFactor,
    VkBlendFactor dstColorBlendFactor,
    VkBlendOp colorBlendOp,
    VkBlendFactor srcAlphaBlendFactor,
    VkBlendFactor dstAlphaBlendFactor,
    VkBlendOp alphaBlendOp,
    VkColorComponentFlags colorWriteMask) {
  const VkPipelineColorBlendAttachmentState state = {
      .blendEnable = blendEnable,
      .srcColorBlendFactor = srcColorBlendFactor,
      .dstColorBlendFactor = dstColorBlendFactor,
      .colorBlendOp = colorBlendOp,
      .srcAlphaBlendFactor = srcAlphaBlendFactor,
      .dstAlphaBlendFactor = dstAlphaBlendFactor,
      .alphaBlendOp = alphaBlendOp,
      .colorWriteMask = colorWriteMask,
  };
  return state;
}

VkPipelineColorBlendStateCreateInfo ivkGetPipelineColorBlendStateCreateInfo(
    uint32_t numAttachments,
    const VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates) {
  const VkPipelineColorBlendStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = numAttachments,
      .pAttachments = colorBlendAttachmentStates,
  };
  return ci;
}

VkImageSubresourceRange ivkGetImageSubresourceRange(VkImageAspectFlags aspectMask) {
  const VkImageSubresourceRange range = {
      .aspectMask = aspectMask,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };
  return range;
}

VkWriteDescriptorSet ivkGetWriteDescriptorSet_ImageInfo(VkDescriptorSet dstSet,
                                                        uint32_t dstBinding,
                                                        VkDescriptorType descriptorType,
                                                        uint32_t numDescriptors,
                                                        const VkDescriptorImageInfo* pImageInfo) {
  const VkWriteDescriptorSet set = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = NULL,
      .dstSet = dstSet,
      .dstBinding = dstBinding,
      .dstArrayElement = 0,
      .descriptorCount = numDescriptors,
      .descriptorType = descriptorType,
      .pImageInfo = pImageInfo,
      .pBufferInfo = NULL,
      .pTexelBufferView = NULL,
  };
  return set;
}

VkWriteDescriptorSet ivkGetWriteDescriptorSet_BufferInfo(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    VkDescriptorType descriptorType,
    uint32_t numDescriptors,
    const VkDescriptorBufferInfo* pBufferInfo) {
  const VkWriteDescriptorSet set = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = NULL,
      .dstSet = dstSet,
      .dstBinding = dstBinding,
      .dstArrayElement = 0,
      .descriptorCount = numDescriptors,
      .descriptorType = descriptorType,
      .pImageInfo = NULL,
      .pBufferInfo = pBufferInfo,
      .pTexelBufferView = NULL,
  };
  return set;
}

VkPipelineLayoutCreateInfo ivkGetPipelineLayoutCreateInfo(uint32_t numLayouts,
                                                          const VkDescriptorSetLayout* layouts,
                                                          const VkPushConstantRange* range) {
  const VkPipelineLayoutCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = numLayouts,
      .pSetLayouts = layouts,
      .pushConstantRangeCount = range ? 1u : 0u,
      .pPushConstantRanges = range,
  };
  return ci;
}

VkPushConstantRange ivkGetPushConstantRange(VkShaderStageFlags stageFlags,
                                            size_t offset,
                                            size_t size) {
  const VkPushConstantRange range = {
      .stageFlags = stageFlags,
      .offset = (uint32_t)offset,
      .size = (uint32_t)size,
  };
  return range;
}

VkPipelineShaderStageCreateInfo ivkGetPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                    VkShaderModule shaderModule,
                                                                    const char* entryPoint) {
  const VkPipelineShaderStageCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .flags = 0,
      .stage = stage,
      .module = shaderModule,
      .pName = entryPoint ? entryPoint : "main",
      .pSpecializationInfo = NULL,
  };
  return ci;
}

VkViewport ivkGetViewport(float x, float y, float width, float height) {
  const VkViewport viewport = {
      .x = x,
      .y = y,
      .width = width,
      .height = height,
      .minDepth = 0.0f,
      .maxDepth = +1.0f,
  };
  return viewport;
}

VkRect2D ivkGetRect2D(int32_t x, int32_t y, uint32_t width, uint32_t height) {
  const VkRect2D rect = {
      .offset = {.x = x, .y = y},
      .extent = {.width = width, .height = height},
  };
  return rect;
}

VkResult ivkCreateShaderModuleFromSPIRV(const struct VulkanFunctionTable* vt,
                                        VkDevice device,
                                        const void* dataSPIRV,
                                        size_t size,
                                        VkShaderModule* outShaderModule) {
  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = dataSPIRV,
  };
  return vt->vkCreateShaderModule(device, &ci, NULL, outShaderModule);
}

VkResult ivkCreateGraphicsPipeline(const struct VulkanFunctionTable* vt,
                                   VkDevice device,
                                   VkPipelineCache pipelineCache,
                                   uint32_t numShaderStages,
                                   const VkPipelineShaderStageCreateInfo* shaderStages,
                                   const VkPipelineVertexInputStateCreateInfo* vertexInputState,
                                   const VkPipelineInputAssemblyStateCreateInfo* inputAssemblyState,
                                   const VkPipelineTessellationStateCreateInfo* tessellationState,
                                   const VkPipelineViewportStateCreateInfo* viewportState,
                                   const VkPipelineRasterizationStateCreateInfo* rasterizationState,
                                   const VkPipelineMultisampleStateCreateInfo* multisampleState,
                                   const VkPipelineDepthStencilStateCreateInfo* depthStencilState,
                                   const VkPipelineColorBlendStateCreateInfo* colorBlendState,
                                   const VkPipelineDynamicStateCreateInfo* dynamicState,
                                   VkPipelineLayout pipelineLayout,
                                   VkRenderPass renderPass,
                                   VkPipeline* outPipeline) {
  const VkGraphicsPipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stageCount = numShaderStages,
      .pStages = shaderStages,
      .pVertexInputState = vertexInputState,
      .pInputAssemblyState = inputAssemblyState,
      .pTessellationState = tessellationState,
      .pViewportState = viewportState,
      .pRasterizationState = rasterizationState,
      .pMultisampleState = multisampleState,
      .pDepthStencilState = depthStencilState,
      .pColorBlendState = colorBlendState,
      .pDynamicState = dynamicState,
      .layout = pipelineLayout,
      .renderPass = renderPass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  return vt->vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, NULL, outPipeline);
}

VkResult ivkCreateComputePipeline(const struct VulkanFunctionTable* vt,
                                  VkDevice device,
                                  VkPipelineCache pipelineCache,
                                  const VkPipelineShaderStageCreateInfo* shaderStage,
                                  VkPipelineLayout pipelineLayout,
                                  VkPipeline* outPipeline) {
  const VkComputePipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = *shaderStage,
      .layout = pipelineLayout,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  return vt->vkCreateComputePipelines(device, pipelineCache, 1, &ci, NULL, outPipeline);
}

void ivkImageMemoryBarrier(const struct VulkanFunctionTable* vt,
                           VkCommandBuffer buffer,
                           VkImage image,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           VkImageLayout oldImageLayout,
                           VkImageLayout newImageLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkImageSubresourceRange subresourceRange) {
  const VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = srcAccessMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldImageLayout,
      .newLayout = newImageLayout,
      .image = image,
      .subresourceRange = subresourceRange,
  };
  vt->vkCmdPipelineBarrier(buffer, srcStageMask, dstStageMask, 0, 0, NULL, 0, NULL, 1, &barrier);
}

void ivkBufferBarrier(const struct VulkanFunctionTable* vt,
                      VkCommandBuffer cmdBuffer,
                      VkBuffer buffer,
                      VkBufferUsageFlags usageFlags,
                      VkPipelineStageFlags srcStageMask,
                      VkPipelineStageFlags dstStageMask) {
  VkBufferMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer,
      .offset = 0,
      .size = VK_WHOLE_SIZE,
  };

  if (dstStageMask & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) {
    barrier.dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
  }
  if (usageFlags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
    barrier.dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;
  }
  if (usageFlags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
    barrier.dstAccessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
  }

  vt->vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, 0, 0, NULL, 1, &barrier, 0, NULL);
}

void ivkBufferMemoryBarrier(const struct VulkanFunctionTable* vt,
                            VkCommandBuffer cmdBuffer,
                            VkBuffer buffer,
                            VkAccessFlags srcAccessMask,
                            VkAccessFlags dstAccessMask,
                            VkDeviceSize offset,
                            VkDeviceSize size,
                            VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask) {
  const VkBufferMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = srcAccessMask,
      .dstAccessMask = dstAccessMask,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer,
      .offset = offset,
      .size = size,
  };
  vt->vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, 0, 0, NULL, 1, &barrier, 0, NULL);
}

void ivkCmdBlitImage(const struct VulkanFunctionTable* vt,
                     VkCommandBuffer buffer,
                     VkImage srcImage,
                     VkImage dstImage,
                     VkImageLayout srcImageLayout,
                     VkImageLayout dstImageLayout,
                     const VkOffset3D* srcOffsets,
                     const VkOffset3D* dstOffsets,
                     VkImageSubresourceLayers srcSubresourceRange,
                     VkImageSubresourceLayers dstSubresourceRange,
                     VkFilter filter) {
  const VkImageBlit blit = {
      .srcSubresource = srcSubresourceRange,
      .srcOffsets = {srcOffsets[0], srcOffsets[1]},
      .dstSubresource = dstSubresourceRange,
      .dstOffsets = {dstOffsets[0], dstOffsets[1]},
  };
  vt->vkCmdBlitImage(buffer, srcImage, srcImageLayout, dstImage, dstImageLayout, 1, &blit, filter);
}

VkResult ivkQueuePresent(const struct VulkanFunctionTable* vt,
                         VkQueue graphicsQueue,
                         VkSemaphore waitSemaphore,
                         VkSwapchainKHR swapchain,
                         uint32_t currentSwapchainImageIndex) {
  const VkPresentInfoKHR pi = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &waitSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &currentSwapchainImageIndex,
  };
  return vt->vkQueuePresentKHR(graphicsQueue, &pi);
}

VkResult ivkSetDebugObjectName(const struct VulkanFunctionTable* vt,
                               VkDevice device,
                               VkObjectType type,
                               uint64_t handle,
                               const char* name) {
  // Some Android devices don't have the VK_EXT_debug_utils functions available, even though the
  // extension is supported and has been enabled
  if (!name || !*name || vt->vkSetDebugUtilsObjectNameEXT == NULL) {
    return VK_SUCCESS;
  }

#if VK_EXT_DEBUG_UTILS_SUPPORTED
  const VkDebugUtilsObjectNameInfoEXT ni = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = handle,
      .pObjectName = name,
  };

  return vt->vkSetDebugUtilsObjectNameEXT(device, &ni);
#else
  return VK_SUCCESS;
#endif // VK_EXT_DEBUG_UTILS_SUPPORTED
}

void ivkCmdBeginDebugUtilsLabel(const struct VulkanFunctionTable* vt,
                                VkCommandBuffer buffer,
                                const char* name,
                                const float colorRGBA[4]) {
#if VK_EXT_DEBUG_UTILS_SUPPORTED
  // Some Android devices don't have the VK_EXT_debug_utils functions available, even though the
  // extension is supported and has been enabled
  if (!name || !*name || vt->vkCmdBeginDebugUtilsLabelEXT == NULL) {
    return;
  }

  const VkDebugUtilsLabelEXT label = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = NULL,
      .pLabelName = name,
      .color = {colorRGBA[0], colorRGBA[1], colorRGBA[2], colorRGBA[3]},
  };
  vt->vkCmdBeginDebugUtilsLabelEXT(buffer, &label);
#endif // VK_EXT_DEBUG_UTILS_SUPPORTED
}

void ivkCmdInsertDebugUtilsLabel(const struct VulkanFunctionTable* vt,
                                 VkCommandBuffer buffer,
                                 const char* name,
                                 const float colorRGBA[4]) {
#if VK_EXT_DEBUG_UTILS_SUPPORTED
  // Some Android devices don't have the VK_EXT_debug_utils functions available, even though the
  // extension is supported and has been enabled
  if (!name || !*name || vt->vkCmdInsertDebugUtilsLabelEXT == NULL) {
    return;
  }

  const VkDebugUtilsLabelEXT label = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = NULL,
      .pLabelName = name,
      .color = {colorRGBA[0], colorRGBA[1], colorRGBA[2], colorRGBA[3]},
  };
  vt->vkCmdInsertDebugUtilsLabelEXT(buffer, &label);
#endif // VK_EXT_DEBUG_UTILS_SUPPORTED
}

void ivkCmdEndDebugUtilsLabel(const struct VulkanFunctionTable* vt, VkCommandBuffer buffer) {
#if VK_EXT_DEBUG_UTILS_SUPPORTED
  // Some Android devices don't have the VK_EXT_debug_utils functions available, even though the
  // extension is supported and has been enabled
  if (vt->vkCmdEndDebugUtilsLabelEXT != NULL) {
    vt->vkCmdEndDebugUtilsLabelEXT(buffer);
  }
#endif // VK_EXT_DEBUG_UTILS_SUPPORTED
}

VkVertexInputBindingDescription ivkGetVertexInputBindingDescription(uint32_t binding,
                                                                    uint32_t stride,
                                                                    VkVertexInputRate inputRate) {
  const VkVertexInputBindingDescription desc = {
      .binding = binding,
      .stride = stride,
      .inputRate = inputRate,
  };
  return desc;
}

VkVertexInputAttributeDescription ivkGetVertexInputAttributeDescription(uint32_t location,
                                                                        uint32_t binding,
                                                                        VkFormat format,
                                                                        uint32_t offset) {
  const VkVertexInputAttributeDescription desc = {
      .location = location,
      .binding = binding,
      .format = format,
      .offset = offset,
  };
  return desc;
}

VkBufferImageCopy ivkGetBufferImageCopy2D(uint32_t bufferOffset,
                                          uint32_t bufferRowLength,
                                          const VkRect2D imageRegion,
                                          VkImageSubresourceLayers imageSubresource) {
  const VkBufferImageCopy copy = {
      .bufferOffset = bufferOffset,
      .bufferRowLength = bufferRowLength,
      .bufferImageHeight = 0,
      .imageSubresource = imageSubresource,
      .imageOffset = {.x = imageRegion.offset.x, .y = imageRegion.offset.y, .z = 0},
      .imageExtent = {.width = imageRegion.extent.width,
                      .height = imageRegion.extent.height,
                      .depth = 1u},
  };
  return copy;
}

VkBufferImageCopy ivkGetBufferImageCopy3D(uint32_t bufferOffset,
                                          uint32_t bufferRowLength,
                                          const VkOffset3D offset,
                                          const VkExtent3D extent,
                                          VkImageSubresourceLayers imageSubresource) {
  const VkBufferImageCopy copy = {
      .bufferOffset = bufferOffset,
      .bufferRowLength = bufferRowLength,
      .bufferImageHeight = 0,
      .imageSubresource = imageSubresource,
      .imageOffset = offset,
      .imageExtent = extent,
  };
  return copy;
}

VkImageCopy ivkGetImageCopy2D(VkOffset2D srcDstOffset,
                              VkImageSubresourceLayers srcDstImageSubresource,
                              const VkExtent2D imageRegion) {
  const VkImageCopy copy = {
      .srcSubresource = srcDstImageSubresource,
      .srcOffset = {.x = srcDstOffset.x, .y = srcDstOffset.y, .z = 0},
      .dstSubresource = srcDstImageSubresource,
      .dstOffset = {.x = srcDstOffset.x, .y = srcDstOffset.y, .z = 0},
      .extent = {.width = imageRegion.width, .height = imageRegion.height, .depth = 1u},
  };
  return copy;
}

VkResult ivkVmaCreateAllocator(const struct VulkanFunctionTable* vt,
                               VkPhysicalDevice physDev,
                               VkDevice device,
                               VkInstance instance,
                               uint32_t apiVersion,
                               bool enableBufferDeviceAddress,
                               VkDeviceSize preferredLargeHeapBlockSize,
                               VmaAllocator* outVma) {
  const VmaVulkanFunctions funcs = {
      .vkGetInstanceProcAddr = vt->vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr = vt->vkGetDeviceProcAddr,
      .vkGetPhysicalDeviceProperties = vt->vkGetPhysicalDeviceProperties,
      .vkGetPhysicalDeviceMemoryProperties = vt->vkGetPhysicalDeviceMemoryProperties,
      .vkAllocateMemory = vt->vkAllocateMemory,
      .vkFreeMemory = vt->vkFreeMemory,
      .vkMapMemory = vt->vkMapMemory,
      .vkUnmapMemory = vt->vkUnmapMemory,
      .vkFlushMappedMemoryRanges = vt->vkFlushMappedMemoryRanges,
      .vkInvalidateMappedMemoryRanges = vt->vkInvalidateMappedMemoryRanges,
      .vkBindBufferMemory = vt->vkBindBufferMemory,
      .vkBindImageMemory = vt->vkBindImageMemory,
      .vkGetBufferMemoryRequirements = vt->vkGetBufferMemoryRequirements,
      .vkGetImageMemoryRequirements = vt->vkGetImageMemoryRequirements,
      .vkCreateBuffer = vt->vkCreateBuffer,
      .vkDestroyBuffer = vt->vkDestroyBuffer,
      .vkCreateImage = vt->vkCreateImage,
      .vkDestroyImage = vt->vkDestroyImage,
      .vkCmdCopyBuffer = vt->vkCmdCopyBuffer,

#if VMA_VULKAN_VERSION >= 1001000
      .vkGetBufferMemoryRequirements2KHR = vt->vkGetBufferMemoryRequirements2,
      .vkGetImageMemoryRequirements2KHR = vt->vkGetImageMemoryRequirements2,
      .vkBindBufferMemory2KHR = vt->vkBindBufferMemory2,
      .vkBindImageMemory2KHR = vt->vkBindImageMemory2,
      .vkGetPhysicalDeviceMemoryProperties2KHR = vt->vkGetPhysicalDeviceMemoryProperties2,
#endif

#if VMA_VULKAN_VERSION >= 1003000
      .vkGetDeviceBufferMemoryRequirements = vt->vkGetDeviceBufferMemoryRequirements,
      .vkGetDeviceImageMemoryRequirements = vt->vkGetDeviceImageMemoryRequirements,
#endif
  };

  const VmaAllocatorCreateInfo ci = {
      .flags = enableBufferDeviceAddress ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0,
      .physicalDevice = physDev,
      .device = device,
      .preferredLargeHeapBlockSize = preferredLargeHeapBlockSize,
      .pAllocationCallbacks = NULL,
      .pDeviceMemoryCallbacks = NULL,
      .pHeapSizeLimit = NULL,
      .pVulkanFunctions = &funcs,
      .instance = instance,
      .vulkanApiVersion = apiVersion,
  };
  return vmaCreateAllocator(&ci, outVma);
}

void ivkUpdateGlslangResource(glslang_resource_t* res, const VkPhysicalDeviceProperties* props) {
  const VkPhysicalDeviceLimits* limits = props ? &props->limits : NULL;
  if (!limits || !res) {
    return;
  }

  res->max_vertex_attribs = (int)limits->maxVertexInputAttributes;
  res->max_clip_distances = (int)limits->maxClipDistances;
  res->max_compute_work_group_count_x = (int)limits->maxComputeWorkGroupCount[0];
  res->max_compute_work_group_count_y = (int)limits->maxComputeWorkGroupCount[1];
  res->max_compute_work_group_count_z = (int)limits->maxComputeWorkGroupCount[2];
  res->max_compute_work_group_size_x = (int)limits->maxComputeWorkGroupSize[0];
  res->max_compute_work_group_size_y = (int)limits->maxComputeWorkGroupSize[1];
  res->max_compute_work_group_size_z = (int)limits->maxComputeWorkGroupSize[2];
  res->max_vertex_output_components = (int)limits->maxVertexOutputComponents;
  res->max_geometry_input_components = (int)limits->maxGeometryInputComponents;
  res->max_geometry_output_components = (int)limits->maxGeometryOutputComponents;
  res->max_fragment_input_components = (int)limits->maxFragmentInputComponents;
  res->max_geometry_output_vertices = (int)limits->maxGeometryOutputVertices;
  res->max_geometry_total_output_components = (int)limits->maxGeometryTotalOutputComponents;
  res->max_tess_control_input_components =
      (int)limits->maxTessellationControlPerVertexInputComponents;
  res->max_tess_control_output_components =
      (int)limits->maxTessellationControlPerVertexOutputComponents;
  res->max_tess_evaluation_input_components = (int)limits->maxTessellationEvaluationInputComponents;
  res->max_tess_evaluation_output_components =
      (int)limits->maxTessellationEvaluationOutputComponents;
  res->max_viewports = (int)limits->maxViewports;
  res->max_cull_distances = (int)limits->maxCullDistances;
  res->max_combined_clip_and_cull_distances = (int)limits->maxCombinedClipAndCullDistances;
}
