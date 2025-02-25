/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanFunctionTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FORCE_USE_STATIC_SWIFTSHADER) && !defined(FORCE_USE_STATIC_SWIFTSHADER_DISABLED)
extern PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName);
#endif

int loadVulkanLoaderFunctions(struct VulkanFunctionTable* table, PFN_vkGetInstanceProcAddr load) {
  /* IGL_GENERATE_LOAD_LOADER_TABLE */

#if defined(FORCE_USE_STATIC_SWIFTSHADER) && !defined(FORCE_USE_STATIC_SWIFTSHADER_DISABLED)
  if (table->vkGetInstanceProcAddr == NULL) {
    table->vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    load = table->vkGetInstanceProcAddr;
  }
#endif

  if (!load) {
    return 0;
  }
#if defined(VK_VERSION_1_0)
  table->vkCreateInstance = (PFN_vkCreateInstance)load(NULL, "vkCreateInstance");
  table->vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)load(
      NULL, "vkEnumerateInstanceExtensionProperties");
  table->vkEnumerateInstanceLayerProperties =
      (PFN_vkEnumerateInstanceLayerProperties)load(NULL, "vkEnumerateInstanceLayerProperties");
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
  table->vkEnumerateInstanceVersion =
      (PFN_vkEnumerateInstanceVersion)load(NULL, "vkEnumerateInstanceVersion");
#endif /* defined(VK_VERSION_1_1) */
  /* IGL_GENERATE_LOAD_LOADER_TABLE */
  return 1;
}

void loadVulkanInstanceFunctions(struct VulkanFunctionTable* table,
                                 VkInstance context,
                                 PFN_vkGetInstanceProcAddr load,
                                 VkBool32 enableExtDebugUtils) {
  /* IGL_GENERATE_LOAD_INSTANCE_TABLE */
#if defined(VK_VERSION_1_0)
  table->vkCreateDevice = (PFN_vkCreateDevice)load(context, "vkCreateDevice");
  table->vkDestroyInstance = (PFN_vkDestroyInstance)load(context, "vkDestroyInstance");
  table->vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)load(
      context, "vkEnumerateDeviceExtensionProperties");
  table->vkEnumerateDeviceLayerProperties =
      (PFN_vkEnumerateDeviceLayerProperties)load(context, "vkEnumerateDeviceLayerProperties");
  table->vkEnumeratePhysicalDevices =
      (PFN_vkEnumeratePhysicalDevices)load(context, "vkEnumeratePhysicalDevices");
  table->vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)load(context, "vkGetDeviceProcAddr");
  table->vkGetPhysicalDeviceFeatures =
      (PFN_vkGetPhysicalDeviceFeatures)load(context, "vkGetPhysicalDeviceFeatures");
  table->vkGetPhysicalDeviceFormatProperties =
      (PFN_vkGetPhysicalDeviceFormatProperties)load(context, "vkGetPhysicalDeviceFormatProperties");
  table->vkGetPhysicalDeviceImageFormatProperties =
      (PFN_vkGetPhysicalDeviceImageFormatProperties)load(
          context, "vkGetPhysicalDeviceImageFormatProperties");
  table->vkGetPhysicalDeviceMemoryProperties =
      (PFN_vkGetPhysicalDeviceMemoryProperties)load(context, "vkGetPhysicalDeviceMemoryProperties");
  table->vkGetPhysicalDeviceProperties =
      (PFN_vkGetPhysicalDeviceProperties)load(context, "vkGetPhysicalDeviceProperties");
  table->vkGetPhysicalDeviceQueueFamilyProperties =
      (PFN_vkGetPhysicalDeviceQueueFamilyProperties)load(
          context, "vkGetPhysicalDeviceQueueFamilyProperties");
  table->vkGetPhysicalDeviceSparseImageFormatProperties =
      (PFN_vkGetPhysicalDeviceSparseImageFormatProperties)load(
          context, "vkGetPhysicalDeviceSparseImageFormatProperties");
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
  table->vkEnumeratePhysicalDeviceGroups =
      (PFN_vkEnumeratePhysicalDeviceGroups)load(context, "vkEnumeratePhysicalDeviceGroups");
  table->vkGetPhysicalDeviceExternalBufferProperties =
      (PFN_vkGetPhysicalDeviceExternalBufferProperties)load(
          context, "vkGetPhysicalDeviceExternalBufferProperties");
  table->vkGetPhysicalDeviceExternalFenceProperties =
      (PFN_vkGetPhysicalDeviceExternalFenceProperties)load(
          context, "vkGetPhysicalDeviceExternalFenceProperties");
  table->vkGetPhysicalDeviceExternalSemaphoreProperties =
      (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)load(
          context, "vkGetPhysicalDeviceExternalSemaphoreProperties");
  table->vkGetPhysicalDeviceFeatures2 =
      (PFN_vkGetPhysicalDeviceFeatures2)load(context, "vkGetPhysicalDeviceFeatures2");
  table->vkGetPhysicalDeviceFormatProperties2 = (PFN_vkGetPhysicalDeviceFormatProperties2)load(
      context, "vkGetPhysicalDeviceFormatProperties2");
  table->vkGetPhysicalDeviceImageFormatProperties2 =
      (PFN_vkGetPhysicalDeviceImageFormatProperties2)load(
          context, "vkGetPhysicalDeviceImageFormatProperties2");
  table->vkGetPhysicalDeviceMemoryProperties2 = (PFN_vkGetPhysicalDeviceMemoryProperties2)load(
      context, "vkGetPhysicalDeviceMemoryProperties2");
  table->vkGetPhysicalDeviceProperties2 =
      (PFN_vkGetPhysicalDeviceProperties2)load(context, "vkGetPhysicalDeviceProperties2");
  table->vkGetPhysicalDeviceQueueFamilyProperties2 =
      (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)load(
          context, "vkGetPhysicalDeviceQueueFamilyProperties2");
  table->vkGetPhysicalDeviceSparseImageFormatProperties2 =
      (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2)load(
          context, "vkGetPhysicalDeviceSparseImageFormatProperties2");
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_3)
  table->vkGetPhysicalDeviceToolProperties =
      (PFN_vkGetPhysicalDeviceToolProperties)load(context, "vkGetPhysicalDeviceToolProperties");
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_EXT_acquire_drm_display)
  table->vkAcquireDrmDisplayEXT =
      (PFN_vkAcquireDrmDisplayEXT)load(context, "vkAcquireDrmDisplayEXT");
  table->vkGetDrmDisplayEXT = (PFN_vkGetDrmDisplayEXT)load(context, "vkGetDrmDisplayEXT");
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
  table->vkAcquireXlibDisplayEXT =
      (PFN_vkAcquireXlibDisplayEXT)load(context, "vkAcquireXlibDisplayEXT");
  table->vkGetRandROutputDisplayEXT =
      (PFN_vkGetRandROutputDisplayEXT)load(context, "vkGetRandROutputDisplayEXT");
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_calibrated_timestamps)
  table->vkGetPhysicalDeviceCalibrateableTimeDomainsEXT =
      (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)load(
          context, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_debug_report)
  table->vkCreateDebugReportCallbackEXT =
      (PFN_vkCreateDebugReportCallbackEXT)load(context, "vkCreateDebugReportCallbackEXT");
  table->vkDebugReportMessageEXT =
      (PFN_vkDebugReportMessageEXT)load(context, "vkDebugReportMessageEXT");
  table->vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)load(context, "vkDestroyDebugReportCallbackEXT");
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
  if (enableExtDebugUtils) {
    table->vkCmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)load(context, "vkCmdBeginDebugUtilsLabelEXT");
    table->vkCmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)load(context, "vkCmdEndDebugUtilsLabelEXT");
    table->vkCmdInsertDebugUtilsLabelEXT =
        (PFN_vkCmdInsertDebugUtilsLabelEXT)load(context, "vkCmdInsertDebugUtilsLabelEXT");
    table->vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)load(context, "vkCreateDebugUtilsMessengerEXT");
    table->vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)load(context, "vkDestroyDebugUtilsMessengerEXT");
    table->vkQueueBeginDebugUtilsLabelEXT =
        (PFN_vkQueueBeginDebugUtilsLabelEXT)load(context, "vkQueueBeginDebugUtilsLabelEXT");
    table->vkQueueEndDebugUtilsLabelEXT =
        (PFN_vkQueueEndDebugUtilsLabelEXT)load(context, "vkQueueEndDebugUtilsLabelEXT");
    table->vkQueueInsertDebugUtilsLabelEXT =
        (PFN_vkQueueInsertDebugUtilsLabelEXT)load(context, "vkQueueInsertDebugUtilsLabelEXT");
    table->vkSetDebugUtilsObjectNameEXT =
        (PFN_vkSetDebugUtilsObjectNameEXT)load(context, "vkSetDebugUtilsObjectNameEXT");
    table->vkSetDebugUtilsObjectTagEXT =
        (PFN_vkSetDebugUtilsObjectTagEXT)load(context, "vkSetDebugUtilsObjectTagEXT");
    table->vkSubmitDebugUtilsMessageEXT =
        (PFN_vkSubmitDebugUtilsMessageEXT)load(context, "vkSubmitDebugUtilsMessageEXT");
  }
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_direct_mode_display)
  table->vkReleaseDisplayEXT = (PFN_vkReleaseDisplayEXT)load(context, "vkReleaseDisplayEXT");
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
  table->vkCreateDirectFBSurfaceEXT =
      (PFN_vkCreateDirectFBSurfaceEXT)load(context, "vkCreateDirectFBSurfaceEXT");
  table->vkGetPhysicalDeviceDirectFBPresentationSupportEXT =
      (PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT)load(
          context, "vkGetPhysicalDeviceDirectFBPresentationSupportEXT");
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_display_surface_counter)
  table->vkGetPhysicalDeviceSurfaceCapabilities2EXT =
      (PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT)load(
          context, "vkGetPhysicalDeviceSurfaceCapabilities2EXT");
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_full_screen_exclusive)
  table->vkGetPhysicalDeviceSurfacePresentModes2EXT =
      (PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT)load(
          context, "vkGetPhysicalDeviceSurfacePresentModes2EXT");
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_headless_surface)
  table->vkCreateHeadlessSurfaceEXT =
      (PFN_vkCreateHeadlessSurfaceEXT)load(context, "vkCreateHeadlessSurfaceEXT");
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_metal_surface)
  table->vkCreateMetalSurfaceEXT =
      (PFN_vkCreateMetalSurfaceEXT)load(context, "vkCreateMetalSurfaceEXT");
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_sample_locations)
  table->vkGetPhysicalDeviceMultisamplePropertiesEXT =
      (PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT)load(
          context, "vkGetPhysicalDeviceMultisamplePropertiesEXT");
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_tooling_info)
  table->vkGetPhysicalDeviceToolPropertiesEXT = (PFN_vkGetPhysicalDeviceToolPropertiesEXT)load(
      context, "vkGetPhysicalDeviceToolPropertiesEXT");
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_FUCHSIA_imagepipe_surface)
  table->vkCreateImagePipeSurfaceFUCHSIA =
      (PFN_vkCreateImagePipeSurfaceFUCHSIA)load(context, "vkCreateImagePipeSurfaceFUCHSIA");
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
  table->vkCreateStreamDescriptorSurfaceGGP =
      (PFN_vkCreateStreamDescriptorSurfaceGGP)load(context, "vkCreateStreamDescriptorSurfaceGGP");
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_KHR_android_surface)
  table->vkCreateAndroidSurfaceKHR =
      (PFN_vkCreateAndroidSurfaceKHR)load(context, "vkCreateAndroidSurfaceKHR");
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_device_group_creation)
  table->vkEnumeratePhysicalDeviceGroupsKHR =
      (PFN_vkEnumeratePhysicalDeviceGroupsKHR)load(context, "vkEnumeratePhysicalDeviceGroupsKHR");
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
  table->vkCreateDisplayModeKHR =
      (PFN_vkCreateDisplayModeKHR)load(context, "vkCreateDisplayModeKHR");
  table->vkCreateDisplayPlaneSurfaceKHR =
      (PFN_vkCreateDisplayPlaneSurfaceKHR)load(context, "vkCreateDisplayPlaneSurfaceKHR");
  table->vkGetDisplayModePropertiesKHR =
      (PFN_vkGetDisplayModePropertiesKHR)load(context, "vkGetDisplayModePropertiesKHR");
  table->vkGetDisplayPlaneCapabilitiesKHR =
      (PFN_vkGetDisplayPlaneCapabilitiesKHR)load(context, "vkGetDisplayPlaneCapabilitiesKHR");
  table->vkGetDisplayPlaneSupportedDisplaysKHR = (PFN_vkGetDisplayPlaneSupportedDisplaysKHR)load(
      context, "vkGetDisplayPlaneSupportedDisplaysKHR");
  table->vkGetPhysicalDeviceDisplayPlanePropertiesKHR =
      (PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)load(
          context, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
  table->vkGetPhysicalDeviceDisplayPropertiesKHR =
      (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)load(context,
                                                        "vkGetPhysicalDeviceDisplayPropertiesKHR");
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_external_fence_capabilities)
  table->vkGetPhysicalDeviceExternalFencePropertiesKHR =
      (PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR)load(
          context, "vkGetPhysicalDeviceExternalFencePropertiesKHR");
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_memory_capabilities)
  table->vkGetPhysicalDeviceExternalBufferPropertiesKHR =
      (PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR)load(
          context, "vkGetPhysicalDeviceExternalBufferPropertiesKHR");
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_semaphore_capabilities)
  table->vkGetPhysicalDeviceExternalSemaphorePropertiesKHR =
      (PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)load(
          context, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_fragment_shading_rate)
  table->vkGetPhysicalDeviceFragmentShadingRatesKHR =
      (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)load(
          context, "vkGetPhysicalDeviceFragmentShadingRatesKHR");
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
  table->vkGetDisplayModeProperties2KHR =
      (PFN_vkGetDisplayModeProperties2KHR)load(context, "vkGetDisplayModeProperties2KHR");
  table->vkGetDisplayPlaneCapabilities2KHR =
      (PFN_vkGetDisplayPlaneCapabilities2KHR)load(context, "vkGetDisplayPlaneCapabilities2KHR");
  table->vkGetPhysicalDeviceDisplayPlaneProperties2KHR =
      (PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR)load(
          context, "vkGetPhysicalDeviceDisplayPlaneProperties2KHR");
  table->vkGetPhysicalDeviceDisplayProperties2KHR =
      (PFN_vkGetPhysicalDeviceDisplayProperties2KHR)load(
          context, "vkGetPhysicalDeviceDisplayProperties2KHR");
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_physical_device_properties2)
  table->vkGetPhysicalDeviceFeatures2KHR =
      (PFN_vkGetPhysicalDeviceFeatures2KHR)load(context, "vkGetPhysicalDeviceFeatures2KHR");
  table->vkGetPhysicalDeviceFormatProperties2KHR =
      (PFN_vkGetPhysicalDeviceFormatProperties2KHR)load(context,
                                                        "vkGetPhysicalDeviceFormatProperties2KHR");
  table->vkGetPhysicalDeviceImageFormatProperties2KHR =
      (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)load(
          context, "vkGetPhysicalDeviceImageFormatProperties2KHR");
  table->vkGetPhysicalDeviceMemoryProperties2KHR =
      (PFN_vkGetPhysicalDeviceMemoryProperties2KHR)load(context,
                                                        "vkGetPhysicalDeviceMemoryProperties2KHR");
  table->vkGetPhysicalDeviceProperties2KHR =
      (PFN_vkGetPhysicalDeviceProperties2KHR)load(context, "vkGetPhysicalDeviceProperties2KHR");
  table->vkGetPhysicalDeviceQueueFamilyProperties2KHR =
      (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)load(
          context, "vkGetPhysicalDeviceQueueFamilyProperties2KHR");
  table->vkGetPhysicalDeviceSparseImageFormatProperties2KHR =
      (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)load(
          context, "vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
  table->vkGetPhysicalDeviceSurfaceCapabilities2KHR =
      (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)load(
          context, "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
  table->vkGetPhysicalDeviceSurfaceFormats2KHR = (PFN_vkGetPhysicalDeviceSurfaceFormats2KHR)load(
      context, "vkGetPhysicalDeviceSurfaceFormats2KHR");
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_performance_query)
  table->vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR =
      (PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)load(
          context, "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
  table->vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR =
      (PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)load(
          context, "vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_surface)
  table->vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)load(context, "vkDestroySurfaceKHR");
  table->vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
      (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)load(
          context, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
  table->vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)load(
      context, "vkGetPhysicalDeviceSurfaceFormatsKHR");
  table->vkGetPhysicalDeviceSurfacePresentModesKHR =
      (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)load(
          context, "vkGetPhysicalDeviceSurfacePresentModesKHR");
  table->vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)load(
      context, "vkGetPhysicalDeviceSurfaceSupportKHR");
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_video_queue)
  table->vkGetPhysicalDeviceVideoCapabilitiesKHR =
      (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)load(context,
                                                        "vkGetPhysicalDeviceVideoCapabilitiesKHR");
  table->vkGetPhysicalDeviceVideoFormatPropertiesKHR =
      (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)load(
          context, "vkGetPhysicalDeviceVideoFormatPropertiesKHR");
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
  table->vkCreateWaylandSurfaceKHR =
      (PFN_vkCreateWaylandSurfaceKHR)load(context, "vkCreateWaylandSurfaceKHR");
  table->vkGetPhysicalDeviceWaylandPresentationSupportKHR =
      (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)load(
          context, "vkGetPhysicalDeviceWaylandPresentationSupportKHR");
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
  table->vkCreateWin32SurfaceKHR =
      (PFN_vkCreateWin32SurfaceKHR)load(context, "vkCreateWin32SurfaceKHR");
  table->vkGetPhysicalDeviceWin32PresentationSupportKHR =
      (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)load(
          context, "vkGetPhysicalDeviceWin32PresentationSupportKHR");
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
  table->vkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)load(context, "vkCreateXcbSurfaceKHR");
  table->vkGetPhysicalDeviceXcbPresentationSupportKHR =
      (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)load(
          context, "vkGetPhysicalDeviceXcbPresentationSupportKHR");
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
  table->vkCreateXlibSurfaceKHR =
      (PFN_vkCreateXlibSurfaceKHR)load(context, "vkCreateXlibSurfaceKHR");
  table->vkGetPhysicalDeviceXlibPresentationSupportKHR =
      (PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR)load(
          context, "vkGetPhysicalDeviceXlibPresentationSupportKHR");
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
  table->vkCreateIOSSurfaceMVK = (PFN_vkCreateIOSSurfaceMVK)load(context, "vkCreateIOSSurfaceMVK");
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
  table->vkCreateMacOSSurfaceMVK =
      (PFN_vkCreateMacOSSurfaceMVK)load(context, "vkCreateMacOSSurfaceMVK");
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
  table->vkCreateViSurfaceNN = (PFN_vkCreateViSurfaceNN)load(context, "vkCreateViSurfaceNN");
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NV_acquire_winrt_display)
  table->vkAcquireWinrtDisplayNV =
      (PFN_vkAcquireWinrtDisplayNV)load(context, "vkAcquireWinrtDisplayNV");
  table->vkGetWinrtDisplayNV = (PFN_vkGetWinrtDisplayNV)load(context, "vkGetWinrtDisplayNV");
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_cooperative_matrix)
  table->vkGetPhysicalDeviceCooperativeMatrixPropertiesNV =
      (PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)load(
          context, "vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_coverage_reduction_mode)
  table->vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV =
      (PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)load(
          context, "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_external_memory_capabilities)
  table->vkGetPhysicalDeviceExternalImageFormatPropertiesNV =
      (PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV)load(
          context, "vkGetPhysicalDeviceExternalImageFormatPropertiesNV");
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_QNX_screen_surface)
  table->vkCreateScreenSurfaceQNX =
      (PFN_vkCreateScreenSurfaceQNX)load(context, "vkCreateScreenSurfaceQNX");
  table->vkGetPhysicalDeviceScreenPresentationSupportQNX =
      (PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX)load(
          context, "vkGetPhysicalDeviceScreenPresentationSupportQNX");
#endif /* defined(VK_QNX_screen_surface) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  table->vkGetPhysicalDevicePresentRectanglesKHR =
      (PFN_vkGetPhysicalDevicePresentRectanglesKHR)load(context,
                                                        "vkGetPhysicalDevicePresentRectanglesKHR");
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) \
          && defined(VK_VERSION_1_1)) */
  /* IGL_GENERATE_LOAD_INSTANCE_TABLE */
}

void loadVulkanDeviceFunctions(struct VulkanFunctionTable* table,
                               VkDevice context,
                               PFN_vkGetDeviceProcAddr load) {
/* IGL_GENERATE_LOAD_DEVICE_TABLE */
#if defined(VK_VERSION_1_0)
  table->vkAllocateCommandBuffers =
      (PFN_vkAllocateCommandBuffers)load(context, "vkAllocateCommandBuffers");
  table->vkAllocateDescriptorSets =
      (PFN_vkAllocateDescriptorSets)load(context, "vkAllocateDescriptorSets");
  table->vkAllocateMemory = (PFN_vkAllocateMemory)load(context, "vkAllocateMemory");
  table->vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)load(context, "vkBeginCommandBuffer");
  table->vkBindBufferMemory = (PFN_vkBindBufferMemory)load(context, "vkBindBufferMemory");
  table->vkBindImageMemory = (PFN_vkBindImageMemory)load(context, "vkBindImageMemory");
  table->vkCmdBeginQuery = (PFN_vkCmdBeginQuery)load(context, "vkCmdBeginQuery");
  table->vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)load(context, "vkCmdBeginRenderPass");
  table->vkCmdBindDescriptorSets =
      (PFN_vkCmdBindDescriptorSets)load(context, "vkCmdBindDescriptorSets");
  table->vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)load(context, "vkCmdBindIndexBuffer");
  table->vkCmdBindPipeline = (PFN_vkCmdBindPipeline)load(context, "vkCmdBindPipeline");
  table->vkCmdBindVertexBuffers =
      (PFN_vkCmdBindVertexBuffers)load(context, "vkCmdBindVertexBuffers");
  table->vkCmdBlitImage = (PFN_vkCmdBlitImage)load(context, "vkCmdBlitImage");
  table->vkCmdClearAttachments = (PFN_vkCmdClearAttachments)load(context, "vkCmdClearAttachments");
  table->vkCmdClearColorImage = (PFN_vkCmdClearColorImage)load(context, "vkCmdClearColorImage");
  table->vkCmdClearDepthStencilImage =
      (PFN_vkCmdClearDepthStencilImage)load(context, "vkCmdClearDepthStencilImage");
  table->vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)load(context, "vkCmdCopyBuffer");
  table->vkCmdCopyBufferToImage =
      (PFN_vkCmdCopyBufferToImage)load(context, "vkCmdCopyBufferToImage");
  table->vkCmdCopyImage = (PFN_vkCmdCopyImage)load(context, "vkCmdCopyImage");
  table->vkCmdCopyImageToBuffer =
      (PFN_vkCmdCopyImageToBuffer)load(context, "vkCmdCopyImageToBuffer");
  table->vkCmdCopyQueryPoolResults =
      (PFN_vkCmdCopyQueryPoolResults)load(context, "vkCmdCopyQueryPoolResults");
  table->vkCmdDispatch = (PFN_vkCmdDispatch)load(context, "vkCmdDispatch");
  table->vkCmdDispatchIndirect = (PFN_vkCmdDispatchIndirect)load(context, "vkCmdDispatchIndirect");
  table->vkCmdDraw = (PFN_vkCmdDraw)load(context, "vkCmdDraw");
  table->vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)load(context, "vkCmdDrawIndexed");
  table->vkCmdDrawIndexedIndirect =
      (PFN_vkCmdDrawIndexedIndirect)load(context, "vkCmdDrawIndexedIndirect");
  table->vkCmdDrawIndirect = (PFN_vkCmdDrawIndirect)load(context, "vkCmdDrawIndirect");
  table->vkCmdEndQuery = (PFN_vkCmdEndQuery)load(context, "vkCmdEndQuery");
  table->vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)load(context, "vkCmdEndRenderPass");
  table->vkCmdExecuteCommands = (PFN_vkCmdExecuteCommands)load(context, "vkCmdExecuteCommands");
  table->vkCmdFillBuffer = (PFN_vkCmdFillBuffer)load(context, "vkCmdFillBuffer");
  table->vkCmdNextSubpass = (PFN_vkCmdNextSubpass)load(context, "vkCmdNextSubpass");
  table->vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)load(context, "vkCmdPipelineBarrier");
  table->vkCmdPushConstants = (PFN_vkCmdPushConstants)load(context, "vkCmdPushConstants");
  table->vkCmdResetEvent = (PFN_vkCmdResetEvent)load(context, "vkCmdResetEvent");
  table->vkCmdResetQueryPool = (PFN_vkCmdResetQueryPool)load(context, "vkCmdResetQueryPool");
  table->vkCmdResolveImage = (PFN_vkCmdResolveImage)load(context, "vkCmdResolveImage");
  table->vkCmdSetBlendConstants =
      (PFN_vkCmdSetBlendConstants)load(context, "vkCmdSetBlendConstants");
  table->vkCmdSetDepthBias = (PFN_vkCmdSetDepthBias)load(context, "vkCmdSetDepthBias");
  table->vkCmdSetDepthBounds = (PFN_vkCmdSetDepthBounds)load(context, "vkCmdSetDepthBounds");
  table->vkCmdSetEvent = (PFN_vkCmdSetEvent)load(context, "vkCmdSetEvent");
  table->vkCmdSetLineWidth = (PFN_vkCmdSetLineWidth)load(context, "vkCmdSetLineWidth");
  table->vkCmdSetScissor = (PFN_vkCmdSetScissor)load(context, "vkCmdSetScissor");
  table->vkCmdSetStencilCompareMask =
      (PFN_vkCmdSetStencilCompareMask)load(context, "vkCmdSetStencilCompareMask");
  table->vkCmdSetStencilReference =
      (PFN_vkCmdSetStencilReference)load(context, "vkCmdSetStencilReference");
  table->vkCmdSetStencilWriteMask =
      (PFN_vkCmdSetStencilWriteMask)load(context, "vkCmdSetStencilWriteMask");
  table->vkCmdSetViewport = (PFN_vkCmdSetViewport)load(context, "vkCmdSetViewport");
  table->vkCmdUpdateBuffer = (PFN_vkCmdUpdateBuffer)load(context, "vkCmdUpdateBuffer");
  table->vkCmdWaitEvents = (PFN_vkCmdWaitEvents)load(context, "vkCmdWaitEvents");
  table->vkCmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)load(context, "vkCmdWriteTimestamp");
  table->vkCreateBuffer = (PFN_vkCreateBuffer)load(context, "vkCreateBuffer");
  table->vkCreateBufferView = (PFN_vkCreateBufferView)load(context, "vkCreateBufferView");
  table->vkCreateCommandPool = (PFN_vkCreateCommandPool)load(context, "vkCreateCommandPool");
  table->vkCreateComputePipelines =
      (PFN_vkCreateComputePipelines)load(context, "vkCreateComputePipelines");
  table->vkCreateDescriptorPool =
      (PFN_vkCreateDescriptorPool)load(context, "vkCreateDescriptorPool");
  table->vkCreateDescriptorSetLayout =
      (PFN_vkCreateDescriptorSetLayout)load(context, "vkCreateDescriptorSetLayout");
  table->vkCreateEvent = (PFN_vkCreateEvent)load(context, "vkCreateEvent");
  table->vkCreateFence = (PFN_vkCreateFence)load(context, "vkCreateFence");
  table->vkCreateFramebuffer = (PFN_vkCreateFramebuffer)load(context, "vkCreateFramebuffer");
  table->vkCreateGraphicsPipelines =
      (PFN_vkCreateGraphicsPipelines)load(context, "vkCreateGraphicsPipelines");
  table->vkCreateImage = (PFN_vkCreateImage)load(context, "vkCreateImage");
  table->vkCreateImageView = (PFN_vkCreateImageView)load(context, "vkCreateImageView");
  table->vkCreatePipelineCache = (PFN_vkCreatePipelineCache)load(context, "vkCreatePipelineCache");
  table->vkCreatePipelineLayout =
      (PFN_vkCreatePipelineLayout)load(context, "vkCreatePipelineLayout");
  table->vkCreateQueryPool = (PFN_vkCreateQueryPool)load(context, "vkCreateQueryPool");
  table->vkCreateRenderPass = (PFN_vkCreateRenderPass)load(context, "vkCreateRenderPass");
  table->vkCreateSampler = (PFN_vkCreateSampler)load(context, "vkCreateSampler");
  table->vkCreateSemaphore = (PFN_vkCreateSemaphore)load(context, "vkCreateSemaphore");
  table->vkCreateShaderModule = (PFN_vkCreateShaderModule)load(context, "vkCreateShaderModule");
  table->vkDestroyBuffer = (PFN_vkDestroyBuffer)load(context, "vkDestroyBuffer");
  table->vkDestroyBufferView = (PFN_vkDestroyBufferView)load(context, "vkDestroyBufferView");
  table->vkDestroyCommandPool = (PFN_vkDestroyCommandPool)load(context, "vkDestroyCommandPool");
  table->vkDestroyDescriptorPool =
      (PFN_vkDestroyDescriptorPool)load(context, "vkDestroyDescriptorPool");
  table->vkDestroyDescriptorSetLayout =
      (PFN_vkDestroyDescriptorSetLayout)load(context, "vkDestroyDescriptorSetLayout");
  table->vkDestroyDevice = (PFN_vkDestroyDevice)load(context, "vkDestroyDevice");
  table->vkDestroyEvent = (PFN_vkDestroyEvent)load(context, "vkDestroyEvent");
  table->vkDestroyFence = (PFN_vkDestroyFence)load(context, "vkDestroyFence");
  table->vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)load(context, "vkDestroyFramebuffer");
  table->vkDestroyImage = (PFN_vkDestroyImage)load(context, "vkDestroyImage");
  table->vkDestroyImageView = (PFN_vkDestroyImageView)load(context, "vkDestroyImageView");
  table->vkDestroyPipeline = (PFN_vkDestroyPipeline)load(context, "vkDestroyPipeline");
  table->vkDestroyPipelineCache =
      (PFN_vkDestroyPipelineCache)load(context, "vkDestroyPipelineCache");
  table->vkDestroyPipelineLayout =
      (PFN_vkDestroyPipelineLayout)load(context, "vkDestroyPipelineLayout");
  table->vkDestroyQueryPool = (PFN_vkDestroyQueryPool)load(context, "vkDestroyQueryPool");
  table->vkDestroyRenderPass = (PFN_vkDestroyRenderPass)load(context, "vkDestroyRenderPass");
  table->vkDestroySampler = (PFN_vkDestroySampler)load(context, "vkDestroySampler");
  table->vkDestroySemaphore = (PFN_vkDestroySemaphore)load(context, "vkDestroySemaphore");
  table->vkDestroyShaderModule = (PFN_vkDestroyShaderModule)load(context, "vkDestroyShaderModule");
  table->vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)load(context, "vkDeviceWaitIdle");
  table->vkEndCommandBuffer = (PFN_vkEndCommandBuffer)load(context, "vkEndCommandBuffer");
  table->vkFlushMappedMemoryRanges =
      (PFN_vkFlushMappedMemoryRanges)load(context, "vkFlushMappedMemoryRanges");
  table->vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)load(context, "vkFreeCommandBuffers");
  table->vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)load(context, "vkFreeDescriptorSets");
  table->vkFreeMemory = (PFN_vkFreeMemory)load(context, "vkFreeMemory");
  table->vkGetBufferMemoryRequirements =
      (PFN_vkGetBufferMemoryRequirements)load(context, "vkGetBufferMemoryRequirements");
  table->vkGetDeviceMemoryCommitment =
      (PFN_vkGetDeviceMemoryCommitment)load(context, "vkGetDeviceMemoryCommitment");
  table->vkGetDeviceQueue = (PFN_vkGetDeviceQueue)load(context, "vkGetDeviceQueue");
  table->vkGetEventStatus = (PFN_vkGetEventStatus)load(context, "vkGetEventStatus");
  table->vkGetFenceStatus = (PFN_vkGetFenceStatus)load(context, "vkGetFenceStatus");
  table->vkGetImageMemoryRequirements =
      (PFN_vkGetImageMemoryRequirements)load(context, "vkGetImageMemoryRequirements");
  table->vkGetImageSparseMemoryRequirements =
      (PFN_vkGetImageSparseMemoryRequirements)load(context, "vkGetImageSparseMemoryRequirements");
  table->vkGetImageSubresourceLayout =
      (PFN_vkGetImageSubresourceLayout)load(context, "vkGetImageSubresourceLayout");
  table->vkGetPipelineCacheData =
      (PFN_vkGetPipelineCacheData)load(context, "vkGetPipelineCacheData");
  table->vkGetQueryPoolResults = (PFN_vkGetQueryPoolResults)load(context, "vkGetQueryPoolResults");
  table->vkGetRenderAreaGranularity =
      (PFN_vkGetRenderAreaGranularity)load(context, "vkGetRenderAreaGranularity");
  table->vkInvalidateMappedMemoryRanges =
      (PFN_vkInvalidateMappedMemoryRanges)load(context, "vkInvalidateMappedMemoryRanges");
  table->vkMapMemory = (PFN_vkMapMemory)load(context, "vkMapMemory");
  table->vkMergePipelineCaches = (PFN_vkMergePipelineCaches)load(context, "vkMergePipelineCaches");
  table->vkQueueBindSparse = (PFN_vkQueueBindSparse)load(context, "vkQueueBindSparse");
  table->vkQueueSubmit = (PFN_vkQueueSubmit)load(context, "vkQueueSubmit");
  table->vkQueueWaitIdle = (PFN_vkQueueWaitIdle)load(context, "vkQueueWaitIdle");
  table->vkResetCommandBuffer = (PFN_vkResetCommandBuffer)load(context, "vkResetCommandBuffer");
  table->vkResetCommandPool = (PFN_vkResetCommandPool)load(context, "vkResetCommandPool");
  table->vkResetDescriptorPool = (PFN_vkResetDescriptorPool)load(context, "vkResetDescriptorPool");
  table->vkResetEvent = (PFN_vkResetEvent)load(context, "vkResetEvent");
  table->vkResetFences = (PFN_vkResetFences)load(context, "vkResetFences");
  table->vkSetEvent = (PFN_vkSetEvent)load(context, "vkSetEvent");
  table->vkUnmapMemory = (PFN_vkUnmapMemory)load(context, "vkUnmapMemory");
  table->vkUpdateDescriptorSets =
      (PFN_vkUpdateDescriptorSets)load(context, "vkUpdateDescriptorSets");
  table->vkWaitForFences = (PFN_vkWaitForFences)load(context, "vkWaitForFences");
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
  table->vkBindBufferMemory2 = (PFN_vkBindBufferMemory2)load(context, "vkBindBufferMemory2");
  table->vkBindImageMemory2 = (PFN_vkBindImageMemory2)load(context, "vkBindImageMemory2");
  table->vkCmdDispatchBase = (PFN_vkCmdDispatchBase)load(context, "vkCmdDispatchBase");
  table->vkCmdSetDeviceMask = (PFN_vkCmdSetDeviceMask)load(context, "vkCmdSetDeviceMask");
  table->vkCreateDescriptorUpdateTemplate =
      (PFN_vkCreateDescriptorUpdateTemplate)load(context, "vkCreateDescriptorUpdateTemplate");
  table->vkCreateSamplerYcbcrConversion =
      (PFN_vkCreateSamplerYcbcrConversion)load(context, "vkCreateSamplerYcbcrConversion");
  table->vkDestroyDescriptorUpdateTemplate =
      (PFN_vkDestroyDescriptorUpdateTemplate)load(context, "vkDestroyDescriptorUpdateTemplate");
  table->vkDestroySamplerYcbcrConversion =
      (PFN_vkDestroySamplerYcbcrConversion)load(context, "vkDestroySamplerYcbcrConversion");
  table->vkGetBufferMemoryRequirements2 =
      (PFN_vkGetBufferMemoryRequirements2)load(context, "vkGetBufferMemoryRequirements2");
  table->vkGetDescriptorSetLayoutSupport =
      (PFN_vkGetDescriptorSetLayoutSupport)load(context, "vkGetDescriptorSetLayoutSupport");
  table->vkGetDeviceGroupPeerMemoryFeatures =
      (PFN_vkGetDeviceGroupPeerMemoryFeatures)load(context, "vkGetDeviceGroupPeerMemoryFeatures");
  table->vkGetDeviceQueue2 = (PFN_vkGetDeviceQueue2)load(context, "vkGetDeviceQueue2");
  table->vkGetImageMemoryRequirements2 =
      (PFN_vkGetImageMemoryRequirements2)load(context, "vkGetImageMemoryRequirements2");
  table->vkGetImageSparseMemoryRequirements2 =
      (PFN_vkGetImageSparseMemoryRequirements2)load(context, "vkGetImageSparseMemoryRequirements2");
  table->vkTrimCommandPool = (PFN_vkTrimCommandPool)load(context, "vkTrimCommandPool");
  table->vkUpdateDescriptorSetWithTemplate =
      (PFN_vkUpdateDescriptorSetWithTemplate)load(context, "vkUpdateDescriptorSetWithTemplate");
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
  table->vkCmdBeginRenderPass2 = (PFN_vkCmdBeginRenderPass2)load(context, "vkCmdBeginRenderPass2");
  table->vkCmdDrawIndexedIndirectCount =
      (PFN_vkCmdDrawIndexedIndirectCount)load(context, "vkCmdDrawIndexedIndirectCount");
  table->vkCmdDrawIndirectCount =
      (PFN_vkCmdDrawIndirectCount)load(context, "vkCmdDrawIndirectCount");
  table->vkCmdEndRenderPass2 = (PFN_vkCmdEndRenderPass2)load(context, "vkCmdEndRenderPass2");
  table->vkCmdNextSubpass2 = (PFN_vkCmdNextSubpass2)load(context, "vkCmdNextSubpass2");
  table->vkCreateRenderPass2 = (PFN_vkCreateRenderPass2)load(context, "vkCreateRenderPass2");
  table->vkGetBufferDeviceAddress =
      (PFN_vkGetBufferDeviceAddress)load(context, "vkGetBufferDeviceAddress");
  table->vkGetBufferOpaqueCaptureAddress =
      (PFN_vkGetBufferOpaqueCaptureAddress)load(context, "vkGetBufferOpaqueCaptureAddress");
  table->vkGetDeviceMemoryOpaqueCaptureAddress = (PFN_vkGetDeviceMemoryOpaqueCaptureAddress)load(
      context, "vkGetDeviceMemoryOpaqueCaptureAddress");
  table->vkGetSemaphoreCounterValue =
      (PFN_vkGetSemaphoreCounterValue)load(context, "vkGetSemaphoreCounterValue");
  table->vkResetQueryPool = (PFN_vkResetQueryPool)load(context, "vkResetQueryPool");
  table->vkSignalSemaphore = (PFN_vkSignalSemaphore)load(context, "vkSignalSemaphore");
  table->vkWaitSemaphores = (PFN_vkWaitSemaphores)load(context, "vkWaitSemaphores");
#endif /* defined(VK_VERSION_1_2) */
#if defined(VK_VERSION_1_3)
  table->vkCmdBeginRendering = (PFN_vkCmdBeginRendering)load(context, "vkCmdBeginRendering");
  table->vkCmdBindVertexBuffers2 =
      (PFN_vkCmdBindVertexBuffers2)load(context, "vkCmdBindVertexBuffers2");
  table->vkCmdBlitImage2 = (PFN_vkCmdBlitImage2)load(context, "vkCmdBlitImage2");
  table->vkCmdCopyBuffer2 = (PFN_vkCmdCopyBuffer2)load(context, "vkCmdCopyBuffer2");
  table->vkCmdCopyBufferToImage2 =
      (PFN_vkCmdCopyBufferToImage2)load(context, "vkCmdCopyBufferToImage2");
  table->vkCmdCopyImage2 = (PFN_vkCmdCopyImage2)load(context, "vkCmdCopyImage2");
  table->vkCmdCopyImageToBuffer2 =
      (PFN_vkCmdCopyImageToBuffer2)load(context, "vkCmdCopyImageToBuffer2");
  table->vkCmdEndRendering = (PFN_vkCmdEndRendering)load(context, "vkCmdEndRendering");
  table->vkCmdPipelineBarrier2 = (PFN_vkCmdPipelineBarrier2)load(context, "vkCmdPipelineBarrier2");
  table->vkCmdResetEvent2 = (PFN_vkCmdResetEvent2)load(context, "vkCmdResetEvent2");
  table->vkCmdResolveImage2 = (PFN_vkCmdResolveImage2)load(context, "vkCmdResolveImage2");
  table->vkCmdSetCullMode = (PFN_vkCmdSetCullMode)load(context, "vkCmdSetCullMode");
  table->vkCmdSetDepthBiasEnable =
      (PFN_vkCmdSetDepthBiasEnable)load(context, "vkCmdSetDepthBiasEnable");
  table->vkCmdSetDepthBoundsTestEnable =
      (PFN_vkCmdSetDepthBoundsTestEnable)load(context, "vkCmdSetDepthBoundsTestEnable");
  table->vkCmdSetDepthCompareOp =
      (PFN_vkCmdSetDepthCompareOp)load(context, "vkCmdSetDepthCompareOp");
  table->vkCmdSetDepthTestEnable =
      (PFN_vkCmdSetDepthTestEnable)load(context, "vkCmdSetDepthTestEnable");
  table->vkCmdSetDepthWriteEnable =
      (PFN_vkCmdSetDepthWriteEnable)load(context, "vkCmdSetDepthWriteEnable");
  table->vkCmdSetEvent2 = (PFN_vkCmdSetEvent2)load(context, "vkCmdSetEvent2");
  table->vkCmdSetFrontFace = (PFN_vkCmdSetFrontFace)load(context, "vkCmdSetFrontFace");
  table->vkCmdSetPrimitiveRestartEnable =
      (PFN_vkCmdSetPrimitiveRestartEnable)load(context, "vkCmdSetPrimitiveRestartEnable");
  table->vkCmdSetPrimitiveTopology =
      (PFN_vkCmdSetPrimitiveTopology)load(context, "vkCmdSetPrimitiveTopology");
  table->vkCmdSetRasterizerDiscardEnable =
      (PFN_vkCmdSetRasterizerDiscardEnable)load(context, "vkCmdSetRasterizerDiscardEnable");
  table->vkCmdSetScissorWithCount =
      (PFN_vkCmdSetScissorWithCount)load(context, "vkCmdSetScissorWithCount");
  table->vkCmdSetStencilOp = (PFN_vkCmdSetStencilOp)load(context, "vkCmdSetStencilOp");
  table->vkCmdSetStencilTestEnable =
      (PFN_vkCmdSetStencilTestEnable)load(context, "vkCmdSetStencilTestEnable");
  table->vkCmdSetViewportWithCount =
      (PFN_vkCmdSetViewportWithCount)load(context, "vkCmdSetViewportWithCount");
  table->vkCmdWaitEvents2 = (PFN_vkCmdWaitEvents2)load(context, "vkCmdWaitEvents2");
  table->vkCmdWriteTimestamp2 = (PFN_vkCmdWriteTimestamp2)load(context, "vkCmdWriteTimestamp2");
  table->vkCreatePrivateDataSlot =
      (PFN_vkCreatePrivateDataSlot)load(context, "vkCreatePrivateDataSlot");
  table->vkDestroyPrivateDataSlot =
      (PFN_vkDestroyPrivateDataSlot)load(context, "vkDestroyPrivateDataSlot");
  table->vkGetDeviceBufferMemoryRequirements =
      (PFN_vkGetDeviceBufferMemoryRequirements)load(context, "vkGetDeviceBufferMemoryRequirements");
  table->vkGetDeviceImageMemoryRequirements =
      (PFN_vkGetDeviceImageMemoryRequirements)load(context, "vkGetDeviceImageMemoryRequirements");
  table->vkGetDeviceImageSparseMemoryRequirements =
      (PFN_vkGetDeviceImageSparseMemoryRequirements)load(
          context, "vkGetDeviceImageSparseMemoryRequirements");
  table->vkGetPrivateData = (PFN_vkGetPrivateData)load(context, "vkGetPrivateData");
  table->vkQueueSubmit2 = (PFN_vkQueueSubmit2)load(context, "vkQueueSubmit2");
  table->vkSetPrivateData = (PFN_vkSetPrivateData)load(context, "vkSetPrivateData");
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_AMD_buffer_marker)
  table->vkCmdWriteBufferMarkerAMD =
      (PFN_vkCmdWriteBufferMarkerAMD)load(context, "vkCmdWriteBufferMarkerAMD");
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_display_native_hdr)
  table->vkSetLocalDimmingAMD = (PFN_vkSetLocalDimmingAMD)load(context, "vkSetLocalDimmingAMD");
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
  table->vkCmdDrawIndexedIndirectCountAMD =
      (PFN_vkCmdDrawIndexedIndirectCountAMD)load(context, "vkCmdDrawIndexedIndirectCountAMD");
  table->vkCmdDrawIndirectCountAMD =
      (PFN_vkCmdDrawIndirectCountAMD)load(context, "vkCmdDrawIndirectCountAMD");
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
  table->vkGetShaderInfoAMD = (PFN_vkGetShaderInfoAMD)load(context, "vkGetShaderInfoAMD");
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
  table->vkGetAndroidHardwareBufferPropertiesANDROID =
      (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)load(
          context, "vkGetAndroidHardwareBufferPropertiesANDROID");
  table->vkGetMemoryAndroidHardwareBufferANDROID =
      (PFN_vkGetMemoryAndroidHardwareBufferANDROID)load(context,
                                                        "vkGetMemoryAndroidHardwareBufferANDROID");
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_EXT_buffer_device_address)
  table->vkGetBufferDeviceAddressEXT =
      (PFN_vkGetBufferDeviceAddressEXT)load(context, "vkGetBufferDeviceAddressEXT");
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
  table->vkGetCalibratedTimestampsEXT =
      (PFN_vkGetCalibratedTimestampsEXT)load(context, "vkGetCalibratedTimestampsEXT");
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
  table->vkCmdSetColorWriteEnableEXT =
      (PFN_vkCmdSetColorWriteEnableEXT)load(context, "vkCmdSetColorWriteEnableEXT");
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
  table->vkCmdBeginConditionalRenderingEXT =
      (PFN_vkCmdBeginConditionalRenderingEXT)load(context, "vkCmdBeginConditionalRenderingEXT");
  table->vkCmdEndConditionalRenderingEXT =
      (PFN_vkCmdEndConditionalRenderingEXT)load(context, "vkCmdEndConditionalRenderingEXT");
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
  table->vkCmdDebugMarkerBeginEXT =
      (PFN_vkCmdDebugMarkerBeginEXT)load(context, "vkCmdDebugMarkerBeginEXT");
  table->vkCmdDebugMarkerEndEXT =
      (PFN_vkCmdDebugMarkerEndEXT)load(context, "vkCmdDebugMarkerEndEXT");
  table->vkCmdDebugMarkerInsertEXT =
      (PFN_vkCmdDebugMarkerInsertEXT)load(context, "vkCmdDebugMarkerInsertEXT");
  table->vkDebugMarkerSetObjectNameEXT =
      (PFN_vkDebugMarkerSetObjectNameEXT)load(context, "vkDebugMarkerSetObjectNameEXT");
  table->vkDebugMarkerSetObjectTagEXT =
      (PFN_vkDebugMarkerSetObjectTagEXT)load(context, "vkDebugMarkerSetObjectTagEXT");
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_discard_rectangles)
  table->vkCmdSetDiscardRectangleEXT =
      (PFN_vkCmdSetDiscardRectangleEXT)load(context, "vkCmdSetDiscardRectangleEXT");
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_display_control)
  table->vkDisplayPowerControlEXT =
      (PFN_vkDisplayPowerControlEXT)load(context, "vkDisplayPowerControlEXT");
  table->vkGetSwapchainCounterEXT =
      (PFN_vkGetSwapchainCounterEXT)load(context, "vkGetSwapchainCounterEXT");
  table->vkRegisterDeviceEventEXT =
      (PFN_vkRegisterDeviceEventEXT)load(context, "vkRegisterDeviceEventEXT");
  table->vkRegisterDisplayEventEXT =
      (PFN_vkRegisterDisplayEventEXT)load(context, "vkRegisterDisplayEventEXT");
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_extended_dynamic_state)
  table->vkCmdBindVertexBuffers2EXT =
      (PFN_vkCmdBindVertexBuffers2EXT)load(context, "vkCmdBindVertexBuffers2EXT");
  table->vkCmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)load(context, "vkCmdSetCullModeEXT");
  table->vkCmdSetDepthBoundsTestEnableEXT =
      (PFN_vkCmdSetDepthBoundsTestEnableEXT)load(context, "vkCmdSetDepthBoundsTestEnableEXT");
  table->vkCmdSetDepthCompareOpEXT =
      (PFN_vkCmdSetDepthCompareOpEXT)load(context, "vkCmdSetDepthCompareOpEXT");
  table->vkCmdSetDepthTestEnableEXT =
      (PFN_vkCmdSetDepthTestEnableEXT)load(context, "vkCmdSetDepthTestEnableEXT");
  table->vkCmdSetDepthWriteEnableEXT =
      (PFN_vkCmdSetDepthWriteEnableEXT)load(context, "vkCmdSetDepthWriteEnableEXT");
  table->vkCmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)load(context, "vkCmdSetFrontFaceEXT");
  table->vkCmdSetPrimitiveTopologyEXT =
      (PFN_vkCmdSetPrimitiveTopologyEXT)load(context, "vkCmdSetPrimitiveTopologyEXT");
  table->vkCmdSetScissorWithCountEXT =
      (PFN_vkCmdSetScissorWithCountEXT)load(context, "vkCmdSetScissorWithCountEXT");
  table->vkCmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)load(context, "vkCmdSetStencilOpEXT");
  table->vkCmdSetStencilTestEnableEXT =
      (PFN_vkCmdSetStencilTestEnableEXT)load(context, "vkCmdSetStencilTestEnableEXT");
  table->vkCmdSetViewportWithCountEXT =
      (PFN_vkCmdSetViewportWithCountEXT)load(context, "vkCmdSetViewportWithCountEXT");
#endif /* defined(VK_EXT_extended_dynamic_state) */
#if defined(VK_EXT_extended_dynamic_state2)
  table->vkCmdSetDepthBiasEnableEXT =
      (PFN_vkCmdSetDepthBiasEnableEXT)load(context, "vkCmdSetDepthBiasEnableEXT");
  table->vkCmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)load(context, "vkCmdSetLogicOpEXT");
  table->vkCmdSetPatchControlPointsEXT =
      (PFN_vkCmdSetPatchControlPointsEXT)load(context, "vkCmdSetPatchControlPointsEXT");
  table->vkCmdSetPrimitiveRestartEnableEXT =
      (PFN_vkCmdSetPrimitiveRestartEnableEXT)load(context, "vkCmdSetPrimitiveRestartEnableEXT");
  table->vkCmdSetRasterizerDiscardEnableEXT =
      (PFN_vkCmdSetRasterizerDiscardEnableEXT)load(context, "vkCmdSetRasterizerDiscardEnableEXT");
#endif /* defined(VK_EXT_extended_dynamic_state2) */
#if defined(VK_EXT_external_memory_host)
  table->vkGetMemoryHostPointerPropertiesEXT =
      (PFN_vkGetMemoryHostPointerPropertiesEXT)load(context, "vkGetMemoryHostPointerPropertiesEXT");
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_full_screen_exclusive)
  table->vkAcquireFullScreenExclusiveModeEXT =
      (PFN_vkAcquireFullScreenExclusiveModeEXT)load(context, "vkAcquireFullScreenExclusiveModeEXT");
  table->vkReleaseFullScreenExclusiveModeEXT =
      (PFN_vkReleaseFullScreenExclusiveModeEXT)load(context, "vkReleaseFullScreenExclusiveModeEXT");
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_hdr_metadata)
  table->vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)load(context, "vkSetHdrMetadataEXT");
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_host_query_reset)
  table->vkResetQueryPoolEXT = (PFN_vkResetQueryPoolEXT)load(context, "vkResetQueryPoolEXT");
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
  table->vkGetImageDrmFormatModifierPropertiesEXT =
      (PFN_vkGetImageDrmFormatModifierPropertiesEXT)load(
          context, "vkGetImageDrmFormatModifierPropertiesEXT");
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
  table->vkCmdSetLineStippleEXT =
      (PFN_vkCmdSetLineStippleEXT)load(context, "vkCmdSetLineStippleEXT");
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_multi_draw)
  table->vkCmdDrawMultiEXT = (PFN_vkCmdDrawMultiEXT)load(context, "vkCmdDrawMultiEXT");
  table->vkCmdDrawMultiIndexedEXT =
      (PFN_vkCmdDrawMultiIndexedEXT)load(context, "vkCmdDrawMultiIndexedEXT");
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_pageable_device_local_memory)
  table->vkSetDeviceMemoryPriorityEXT =
      (PFN_vkSetDeviceMemoryPriorityEXT)load(context, "vkSetDeviceMemoryPriorityEXT");
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_private_data)
  table->vkCreatePrivateDataSlotEXT =
      (PFN_vkCreatePrivateDataSlotEXT)load(context, "vkCreatePrivateDataSlotEXT");
  table->vkDestroyPrivateDataSlotEXT =
      (PFN_vkDestroyPrivateDataSlotEXT)load(context, "vkDestroyPrivateDataSlotEXT");
  table->vkGetPrivateDataEXT = (PFN_vkGetPrivateDataEXT)load(context, "vkGetPrivateDataEXT");
  table->vkSetPrivateDataEXT = (PFN_vkSetPrivateDataEXT)load(context, "vkSetPrivateDataEXT");
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
  table->vkCmdSetSampleLocationsEXT =
      (PFN_vkCmdSetSampleLocationsEXT)load(context, "vkCmdSetSampleLocationsEXT");
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_transform_feedback)
  table->vkCmdBeginQueryIndexedEXT =
      (PFN_vkCmdBeginQueryIndexedEXT)load(context, "vkCmdBeginQueryIndexedEXT");
  table->vkCmdBeginTransformFeedbackEXT =
      (PFN_vkCmdBeginTransformFeedbackEXT)load(context, "vkCmdBeginTransformFeedbackEXT");
  table->vkCmdBindTransformFeedbackBuffersEXT = (PFN_vkCmdBindTransformFeedbackBuffersEXT)load(
      context, "vkCmdBindTransformFeedbackBuffersEXT");
  table->vkCmdDrawIndirectByteCountEXT =
      (PFN_vkCmdDrawIndirectByteCountEXT)load(context, "vkCmdDrawIndirectByteCountEXT");
  table->vkCmdEndQueryIndexedEXT =
      (PFN_vkCmdEndQueryIndexedEXT)load(context, "vkCmdEndQueryIndexedEXT");
  table->vkCmdEndTransformFeedbackEXT =
      (PFN_vkCmdEndTransformFeedbackEXT)load(context, "vkCmdEndTransformFeedbackEXT");
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
  table->vkCreateValidationCacheEXT =
      (PFN_vkCreateValidationCacheEXT)load(context, "vkCreateValidationCacheEXT");
  table->vkDestroyValidationCacheEXT =
      (PFN_vkDestroyValidationCacheEXT)load(context, "vkDestroyValidationCacheEXT");
  table->vkGetValidationCacheDataEXT =
      (PFN_vkGetValidationCacheDataEXT)load(context, "vkGetValidationCacheDataEXT");
  table->vkMergeValidationCachesEXT =
      (PFN_vkMergeValidationCachesEXT)load(context, "vkMergeValidationCachesEXT");
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_EXT_vertex_input_dynamic_state)
  table->vkCmdSetVertexInputEXT =
      (PFN_vkCmdSetVertexInputEXT)load(context, "vkCmdSetVertexInputEXT");
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */
#if defined(VK_FUCHSIA_buffer_collection)
  table->vkCreateBufferCollectionFUCHSIA =
      (PFN_vkCreateBufferCollectionFUCHSIA)load(context, "vkCreateBufferCollectionFUCHSIA");
  table->vkDestroyBufferCollectionFUCHSIA =
      (PFN_vkDestroyBufferCollectionFUCHSIA)load(context, "vkDestroyBufferCollectionFUCHSIA");
  table->vkGetBufferCollectionPropertiesFUCHSIA = (PFN_vkGetBufferCollectionPropertiesFUCHSIA)load(
      context, "vkGetBufferCollectionPropertiesFUCHSIA");
  table->vkSetBufferCollectionBufferConstraintsFUCHSIA =
      (PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA)load(
          context, "vkSetBufferCollectionBufferConstraintsFUCHSIA");
  table->vkSetBufferCollectionImageConstraintsFUCHSIA =
      (PFN_vkSetBufferCollectionImageConstraintsFUCHSIA)load(
          context, "vkSetBufferCollectionImageConstraintsFUCHSIA");
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
  table->vkGetMemoryZirconHandleFUCHSIA =
      (PFN_vkGetMemoryZirconHandleFUCHSIA)load(context, "vkGetMemoryZirconHandleFUCHSIA");
  table->vkGetMemoryZirconHandlePropertiesFUCHSIA =
      (PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)load(
          context, "vkGetMemoryZirconHandlePropertiesFUCHSIA");
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
  table->vkGetSemaphoreZirconHandleFUCHSIA =
      (PFN_vkGetSemaphoreZirconHandleFUCHSIA)load(context, "vkGetSemaphoreZirconHandleFUCHSIA");
  table->vkImportSemaphoreZirconHandleFUCHSIA = (PFN_vkImportSemaphoreZirconHandleFUCHSIA)load(
      context, "vkImportSemaphoreZirconHandleFUCHSIA");
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_GOOGLE_display_timing)
  table->vkGetPastPresentationTimingGOOGLE =
      (PFN_vkGetPastPresentationTimingGOOGLE)load(context, "vkGetPastPresentationTimingGOOGLE");
  table->vkGetRefreshCycleDurationGOOGLE =
      (PFN_vkGetRefreshCycleDurationGOOGLE)load(context, "vkGetRefreshCycleDurationGOOGLE");
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_invocation_mask)
  table->vkCmdBindInvocationMaskHUAWEI =
      (PFN_vkCmdBindInvocationMaskHUAWEI)load(context, "vkCmdBindInvocationMaskHUAWEI");
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading)
  table->vkCmdSubpassShadingHUAWEI =
      (PFN_vkCmdSubpassShadingHUAWEI)load(context, "vkCmdSubpassShadingHUAWEI");
  table->vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI =
      (PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI)load(
          context, "vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI");
#endif /* defined(VK_HUAWEI_subpass_shading) */
#if defined(VK_INTEL_performance_query)
  table->vkAcquirePerformanceConfigurationINTEL = (PFN_vkAcquirePerformanceConfigurationINTEL)load(
      context, "vkAcquirePerformanceConfigurationINTEL");
  table->vkCmdSetPerformanceMarkerINTEL =
      (PFN_vkCmdSetPerformanceMarkerINTEL)load(context, "vkCmdSetPerformanceMarkerINTEL");
  table->vkCmdSetPerformanceOverrideINTEL =
      (PFN_vkCmdSetPerformanceOverrideINTEL)load(context, "vkCmdSetPerformanceOverrideINTEL");
  table->vkCmdSetPerformanceStreamMarkerINTEL = (PFN_vkCmdSetPerformanceStreamMarkerINTEL)load(
      context, "vkCmdSetPerformanceStreamMarkerINTEL");
  table->vkGetPerformanceParameterINTEL =
      (PFN_vkGetPerformanceParameterINTEL)load(context, "vkGetPerformanceParameterINTEL");
  table->vkInitializePerformanceApiINTEL =
      (PFN_vkInitializePerformanceApiINTEL)load(context, "vkInitializePerformanceApiINTEL");
  table->vkQueueSetPerformanceConfigurationINTEL =
      (PFN_vkQueueSetPerformanceConfigurationINTEL)load(context,
                                                        "vkQueueSetPerformanceConfigurationINTEL");
  table->vkReleasePerformanceConfigurationINTEL = (PFN_vkReleasePerformanceConfigurationINTEL)load(
      context, "vkReleasePerformanceConfigurationINTEL");
  table->vkUninitializePerformanceApiINTEL =
      (PFN_vkUninitializePerformanceApiINTEL)load(context, "vkUninitializePerformanceApiINTEL");
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_acceleration_structure)
  table->vkBuildAccelerationStructuresKHR =
      (PFN_vkBuildAccelerationStructuresKHR)load(context, "vkBuildAccelerationStructuresKHR");
  table->vkCmdBuildAccelerationStructuresIndirectKHR =
      (PFN_vkCmdBuildAccelerationStructuresIndirectKHR)load(
          context, "vkCmdBuildAccelerationStructuresIndirectKHR");
  table->vkCmdBuildAccelerationStructuresKHR =
      (PFN_vkCmdBuildAccelerationStructuresKHR)load(context, "vkCmdBuildAccelerationStructuresKHR");
  table->vkCmdCopyAccelerationStructureKHR =
      (PFN_vkCmdCopyAccelerationStructureKHR)load(context, "vkCmdCopyAccelerationStructureKHR");
  table->vkCmdCopyAccelerationStructureToMemoryKHR =
      (PFN_vkCmdCopyAccelerationStructureToMemoryKHR)load(
          context, "vkCmdCopyAccelerationStructureToMemoryKHR");
  table->vkCmdCopyMemoryToAccelerationStructureKHR =
      (PFN_vkCmdCopyMemoryToAccelerationStructureKHR)load(
          context, "vkCmdCopyMemoryToAccelerationStructureKHR");
  table->vkCmdWriteAccelerationStructuresPropertiesKHR =
      (PFN_vkCmdWriteAccelerationStructuresPropertiesKHR)load(
          context, "vkCmdWriteAccelerationStructuresPropertiesKHR");
  table->vkCopyAccelerationStructureKHR =
      (PFN_vkCopyAccelerationStructureKHR)load(context, "vkCopyAccelerationStructureKHR");
  table->vkCopyAccelerationStructureToMemoryKHR = (PFN_vkCopyAccelerationStructureToMemoryKHR)load(
      context, "vkCopyAccelerationStructureToMemoryKHR");
  table->vkCopyMemoryToAccelerationStructureKHR = (PFN_vkCopyMemoryToAccelerationStructureKHR)load(
      context, "vkCopyMemoryToAccelerationStructureKHR");
  table->vkCreateAccelerationStructureKHR =
      (PFN_vkCreateAccelerationStructureKHR)load(context, "vkCreateAccelerationStructureKHR");
  table->vkDestroyAccelerationStructureKHR =
      (PFN_vkDestroyAccelerationStructureKHR)load(context, "vkDestroyAccelerationStructureKHR");
  table->vkGetAccelerationStructureBuildSizesKHR =
      (PFN_vkGetAccelerationStructureBuildSizesKHR)load(context,
                                                        "vkGetAccelerationStructureBuildSizesKHR");
  table->vkGetAccelerationStructureDeviceAddressKHR =
      (PFN_vkGetAccelerationStructureDeviceAddressKHR)load(
          context, "vkGetAccelerationStructureDeviceAddressKHR");
  table->vkGetDeviceAccelerationStructureCompatibilityKHR =
      (PFN_vkGetDeviceAccelerationStructureCompatibilityKHR)load(
          context, "vkGetDeviceAccelerationStructureCompatibilityKHR");
  table->vkWriteAccelerationStructuresPropertiesKHR =
      (PFN_vkWriteAccelerationStructuresPropertiesKHR)load(
          context, "vkWriteAccelerationStructuresPropertiesKHR");
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_bind_memory2)
  table->vkBindBufferMemory2KHR =
      (PFN_vkBindBufferMemory2KHR)load(context, "vkBindBufferMemory2KHR");
  table->vkBindImageMemory2KHR = (PFN_vkBindImageMemory2KHR)load(context, "vkBindImageMemory2KHR");
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
  table->vkGetBufferDeviceAddressKHR =
      (PFN_vkGetBufferDeviceAddressKHR)load(context, "vkGetBufferDeviceAddressKHR");
  table->vkGetBufferOpaqueCaptureAddressKHR =
      (PFN_vkGetBufferOpaqueCaptureAddressKHR)load(context, "vkGetBufferOpaqueCaptureAddressKHR");
  table->vkGetDeviceMemoryOpaqueCaptureAddressKHR =
      (PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR)load(
          context, "vkGetDeviceMemoryOpaqueCaptureAddressKHR");
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_copy_commands2)
  table->vkCmdBlitImage2KHR = (PFN_vkCmdBlitImage2KHR)load(context, "vkCmdBlitImage2KHR");
  table->vkCmdCopyBuffer2KHR = (PFN_vkCmdCopyBuffer2KHR)load(context, "vkCmdCopyBuffer2KHR");
  table->vkCmdCopyBufferToImage2KHR =
      (PFN_vkCmdCopyBufferToImage2KHR)load(context, "vkCmdCopyBufferToImage2KHR");
  table->vkCmdCopyImage2KHR = (PFN_vkCmdCopyImage2KHR)load(context, "vkCmdCopyImage2KHR");
  table->vkCmdCopyImageToBuffer2KHR =
      (PFN_vkCmdCopyImageToBuffer2KHR)load(context, "vkCmdCopyImageToBuffer2KHR");
  table->vkCmdResolveImage2KHR = (PFN_vkCmdResolveImage2KHR)load(context, "vkCmdResolveImage2KHR");
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
  table->vkCmdBeginRenderPass2KHR =
      (PFN_vkCmdBeginRenderPass2KHR)load(context, "vkCmdBeginRenderPass2KHR");
  table->vkCmdEndRenderPass2KHR =
      (PFN_vkCmdEndRenderPass2KHR)load(context, "vkCmdEndRenderPass2KHR");
  table->vkCmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR)load(context, "vkCmdNextSubpass2KHR");
  table->vkCreateRenderPass2KHR =
      (PFN_vkCreateRenderPass2KHR)load(context, "vkCreateRenderPass2KHR");
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
  table->vkCreateDeferredOperationKHR =
      (PFN_vkCreateDeferredOperationKHR)load(context, "vkCreateDeferredOperationKHR");
  table->vkDeferredOperationJoinKHR =
      (PFN_vkDeferredOperationJoinKHR)load(context, "vkDeferredOperationJoinKHR");
  table->vkDestroyDeferredOperationKHR =
      (PFN_vkDestroyDeferredOperationKHR)load(context, "vkDestroyDeferredOperationKHR");
  table->vkGetDeferredOperationMaxConcurrencyKHR =
      (PFN_vkGetDeferredOperationMaxConcurrencyKHR)load(context,
                                                        "vkGetDeferredOperationMaxConcurrencyKHR");
  table->vkGetDeferredOperationResultKHR =
      (PFN_vkGetDeferredOperationResultKHR)load(context, "vkGetDeferredOperationResultKHR");
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
  table->vkCreateDescriptorUpdateTemplateKHR =
      (PFN_vkCreateDescriptorUpdateTemplateKHR)load(context, "vkCreateDescriptorUpdateTemplateKHR");
  table->vkDestroyDescriptorUpdateTemplateKHR = (PFN_vkDestroyDescriptorUpdateTemplateKHR)load(
      context, "vkDestroyDescriptorUpdateTemplateKHR");
  table->vkUpdateDescriptorSetWithTemplateKHR = (PFN_vkUpdateDescriptorSetWithTemplateKHR)load(
      context, "vkUpdateDescriptorSetWithTemplateKHR");
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
  table->vkCmdDispatchBaseKHR = (PFN_vkCmdDispatchBaseKHR)load(context, "vkCmdDispatchBaseKHR");
  table->vkCmdSetDeviceMaskKHR = (PFN_vkCmdSetDeviceMaskKHR)load(context, "vkCmdSetDeviceMaskKHR");
  table->vkGetDeviceGroupPeerMemoryFeaturesKHR = (PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR)load(
      context, "vkGetDeviceGroupPeerMemoryFeaturesKHR");
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_display_swapchain)
  table->vkCreateSharedSwapchainsKHR =
      (PFN_vkCreateSharedSwapchainsKHR)load(context, "vkCreateSharedSwapchainsKHR");
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
  table->vkCmdDrawIndexedIndirectCountKHR =
      (PFN_vkCmdDrawIndexedIndirectCountKHR)load(context, "vkCmdDrawIndexedIndirectCountKHR");
  table->vkCmdDrawIndirectCountKHR =
      (PFN_vkCmdDrawIndirectCountKHR)load(context, "vkCmdDrawIndirectCountKHR");
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
  table->vkCmdBeginRenderingKHR =
      (PFN_vkCmdBeginRenderingKHR)load(context, "vkCmdBeginRenderingKHR");
  table->vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)load(context, "vkCmdEndRenderingKHR");
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_external_fence_fd)
  table->vkGetFenceFdKHR = (PFN_vkGetFenceFdKHR)load(context, "vkGetFenceFdKHR");
  table->vkImportFenceFdKHR = (PFN_vkImportFenceFdKHR)load(context, "vkImportFenceFdKHR");
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
  table->vkGetFenceWin32HandleKHR =
      (PFN_vkGetFenceWin32HandleKHR)load(context, "vkGetFenceWin32HandleKHR");
  table->vkImportFenceWin32HandleKHR =
      (PFN_vkImportFenceWin32HandleKHR)load(context, "vkImportFenceWin32HandleKHR");
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_fd)
  table->vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)load(context, "vkGetMemoryFdKHR");
  table->vkGetMemoryFdPropertiesKHR =
      (PFN_vkGetMemoryFdPropertiesKHR)load(context, "vkGetMemoryFdPropertiesKHR");
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
  table->vkGetMemoryWin32HandleKHR =
      (PFN_vkGetMemoryWin32HandleKHR)load(context, "vkGetMemoryWin32HandleKHR");
  table->vkGetMemoryWin32HandlePropertiesKHR =
      (PFN_vkGetMemoryWin32HandlePropertiesKHR)load(context, "vkGetMemoryWin32HandlePropertiesKHR");
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_fd)
  table->vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)load(context, "vkGetSemaphoreFdKHR");
  table->vkImportSemaphoreFdKHR =
      (PFN_vkImportSemaphoreFdKHR)load(context, "vkImportSemaphoreFdKHR");
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
  table->vkGetSemaphoreWin32HandleKHR =
      (PFN_vkGetSemaphoreWin32HandleKHR)load(context, "vkGetSemaphoreWin32HandleKHR");
  table->vkImportSemaphoreWin32HandleKHR =
      (PFN_vkImportSemaphoreWin32HandleKHR)load(context, "vkImportSemaphoreWin32HandleKHR");
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
  table->vkCmdSetFragmentShadingRateKHR =
      (PFN_vkCmdSetFragmentShadingRateKHR)load(context, "vkCmdSetFragmentShadingRateKHR");
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_memory_requirements2)
  table->vkGetBufferMemoryRequirements2KHR =
      (PFN_vkGetBufferMemoryRequirements2KHR)load(context, "vkGetBufferMemoryRequirements2KHR");
  table->vkGetImageMemoryRequirements2KHR =
      (PFN_vkGetImageMemoryRequirements2KHR)load(context, "vkGetImageMemoryRequirements2KHR");
  table->vkGetImageSparseMemoryRequirements2KHR = (PFN_vkGetImageSparseMemoryRequirements2KHR)load(
      context, "vkGetImageSparseMemoryRequirements2KHR");
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_maintenance1)
  table->vkTrimCommandPoolKHR = (PFN_vkTrimCommandPoolKHR)load(context, "vkTrimCommandPoolKHR");
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
  table->vkGetDescriptorSetLayoutSupportKHR =
      (PFN_vkGetDescriptorSetLayoutSupportKHR)load(context, "vkGetDescriptorSetLayoutSupportKHR");
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
  table->vkGetDeviceBufferMemoryRequirementsKHR = (PFN_vkGetDeviceBufferMemoryRequirementsKHR)load(
      context, "vkGetDeviceBufferMemoryRequirementsKHR");
  table->vkGetDeviceImageMemoryRequirementsKHR = (PFN_vkGetDeviceImageMemoryRequirementsKHR)load(
      context, "vkGetDeviceImageMemoryRequirementsKHR");
  table->vkGetDeviceImageSparseMemoryRequirementsKHR =
      (PFN_vkGetDeviceImageSparseMemoryRequirementsKHR)load(
          context, "vkGetDeviceImageSparseMemoryRequirementsKHR");
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_performance_query)
  table->vkAcquireProfilingLockKHR =
      (PFN_vkAcquireProfilingLockKHR)load(context, "vkAcquireProfilingLockKHR");
  table->vkReleaseProfilingLockKHR =
      (PFN_vkReleaseProfilingLockKHR)load(context, "vkReleaseProfilingLockKHR");
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_executable_properties)
  table->vkGetPipelineExecutableInternalRepresentationsKHR =
      (PFN_vkGetPipelineExecutableInternalRepresentationsKHR)load(
          context, "vkGetPipelineExecutableInternalRepresentationsKHR");
  table->vkGetPipelineExecutablePropertiesKHR = (PFN_vkGetPipelineExecutablePropertiesKHR)load(
      context, "vkGetPipelineExecutablePropertiesKHR");
  table->vkGetPipelineExecutableStatisticsKHR = (PFN_vkGetPipelineExecutableStatisticsKHR)load(
      context, "vkGetPipelineExecutableStatisticsKHR");
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
  table->vkWaitForPresentKHR = (PFN_vkWaitForPresentKHR)load(context, "vkWaitForPresentKHR");
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_push_descriptor)
  table->vkCmdPushDescriptorSetKHR =
      (PFN_vkCmdPushDescriptorSetKHR)load(context, "vkCmdPushDescriptorSetKHR");
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_ray_tracing_pipeline)
  table->vkCmdSetRayTracingPipelineStackSizeKHR = (PFN_vkCmdSetRayTracingPipelineStackSizeKHR)load(
      context, "vkCmdSetRayTracingPipelineStackSizeKHR");
  table->vkCmdTraceRaysIndirectKHR =
      (PFN_vkCmdTraceRaysIndirectKHR)load(context, "vkCmdTraceRaysIndirectKHR");
  table->vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)load(context, "vkCmdTraceRaysKHR");
  table->vkCreateRayTracingPipelinesKHR =
      (PFN_vkCreateRayTracingPipelinesKHR)load(context, "vkCreateRayTracingPipelinesKHR");
  table->vkGetRayTracingCaptureReplayShaderGroupHandlesKHR =
      (PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)load(
          context, "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
  table->vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)load(
      context, "vkGetRayTracingShaderGroupHandlesKHR");
  table->vkGetRayTracingShaderGroupStackSizeKHR = (PFN_vkGetRayTracingShaderGroupStackSizeKHR)load(
      context, "vkGetRayTracingShaderGroupStackSizeKHR");
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
  table->vkCreateSamplerYcbcrConversionKHR =
      (PFN_vkCreateSamplerYcbcrConversionKHR)load(context, "vkCreateSamplerYcbcrConversionKHR");
  table->vkDestroySamplerYcbcrConversionKHR =
      (PFN_vkDestroySamplerYcbcrConversionKHR)load(context, "vkDestroySamplerYcbcrConversionKHR");
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
  table->vkGetSwapchainStatusKHR =
      (PFN_vkGetSwapchainStatusKHR)load(context, "vkGetSwapchainStatusKHR");
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_swapchain)
  table->vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)load(context, "vkAcquireNextImageKHR");
  table->vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)load(context, "vkCreateSwapchainKHR");
  table->vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)load(context, "vkDestroySwapchainKHR");
  table->vkGetSwapchainImagesKHR =
      (PFN_vkGetSwapchainImagesKHR)load(context, "vkGetSwapchainImagesKHR");
  table->vkQueuePresentKHR = (PFN_vkQueuePresentKHR)load(context, "vkQueuePresentKHR");
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_synchronization2)
  table->vkCmdPipelineBarrier2KHR =
      (PFN_vkCmdPipelineBarrier2KHR)load(context, "vkCmdPipelineBarrier2KHR");
  table->vkCmdResetEvent2KHR = (PFN_vkCmdResetEvent2KHR)load(context, "vkCmdResetEvent2KHR");
  table->vkCmdSetEvent2KHR = (PFN_vkCmdSetEvent2KHR)load(context, "vkCmdSetEvent2KHR");
  table->vkCmdWaitEvents2KHR = (PFN_vkCmdWaitEvents2KHR)load(context, "vkCmdWaitEvents2KHR");
  table->vkCmdWriteTimestamp2KHR =
      (PFN_vkCmdWriteTimestamp2KHR)load(context, "vkCmdWriteTimestamp2KHR");
  table->vkQueueSubmit2KHR = (PFN_vkQueueSubmit2KHR)load(context, "vkQueueSubmit2KHR");
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker)
  table->vkCmdWriteBufferMarker2AMD =
      (PFN_vkCmdWriteBufferMarker2AMD)load(context, "vkCmdWriteBufferMarker2AMD");
#endif /* defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker) */
#if defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints)
  table->vkGetQueueCheckpointData2NV =
      (PFN_vkGetQueueCheckpointData2NV)load(context, "vkGetQueueCheckpointData2NV");
#endif /* defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_KHR_timeline_semaphore)
  table->vkGetSemaphoreCounterValueKHR =
      (PFN_vkGetSemaphoreCounterValueKHR)load(context, "vkGetSemaphoreCounterValueKHR");
  table->vkSignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)load(context, "vkSignalSemaphoreKHR");
  table->vkWaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)load(context, "vkWaitSemaphoresKHR");
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
  table->vkCmdDecodeVideoKHR = (PFN_vkCmdDecodeVideoKHR)load(context, "vkCmdDecodeVideoKHR");
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
  table->vkCmdEncodeVideoKHR = (PFN_vkCmdEncodeVideoKHR)load(context, "vkCmdEncodeVideoKHR");
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
  table->vkBindVideoSessionMemoryKHR =
      (PFN_vkBindVideoSessionMemoryKHR)load(context, "vkBindVideoSessionMemoryKHR");
  table->vkCmdBeginVideoCodingKHR =
      (PFN_vkCmdBeginVideoCodingKHR)load(context, "vkCmdBeginVideoCodingKHR");
  table->vkCmdControlVideoCodingKHR =
      (PFN_vkCmdControlVideoCodingKHR)load(context, "vkCmdControlVideoCodingKHR");
  table->vkCmdEndVideoCodingKHR =
      (PFN_vkCmdEndVideoCodingKHR)load(context, "vkCmdEndVideoCodingKHR");
  table->vkCreateVideoSessionKHR =
      (PFN_vkCreateVideoSessionKHR)load(context, "vkCreateVideoSessionKHR");
  table->vkCreateVideoSessionParametersKHR =
      (PFN_vkCreateVideoSessionParametersKHR)load(context, "vkCreateVideoSessionParametersKHR");
  table->vkDestroyVideoSessionKHR =
      (PFN_vkDestroyVideoSessionKHR)load(context, "vkDestroyVideoSessionKHR");
  table->vkDestroyVideoSessionParametersKHR =
      (PFN_vkDestroyVideoSessionParametersKHR)load(context, "vkDestroyVideoSessionParametersKHR");
  table->vkGetVideoSessionMemoryRequirementsKHR = (PFN_vkGetVideoSessionMemoryRequirementsKHR)load(
      context, "vkGetVideoSessionMemoryRequirementsKHR");
  table->vkUpdateVideoSessionParametersKHR =
      (PFN_vkUpdateVideoSessionParametersKHR)load(context, "vkUpdateVideoSessionParametersKHR");
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_NVX_binary_import)
  table->vkCmdCuLaunchKernelNVX =
      (PFN_vkCmdCuLaunchKernelNVX)load(context, "vkCmdCuLaunchKernelNVX");
  table->vkCreateCuFunctionNVX = (PFN_vkCreateCuFunctionNVX)load(context, "vkCreateCuFunctionNVX");
  table->vkCreateCuModuleNVX = (PFN_vkCreateCuModuleNVX)load(context, "vkCreateCuModuleNVX");
  table->vkDestroyCuFunctionNVX =
      (PFN_vkDestroyCuFunctionNVX)load(context, "vkDestroyCuFunctionNVX");
  table->vkDestroyCuModuleNVX = (PFN_vkDestroyCuModuleNVX)load(context, "vkDestroyCuModuleNVX");
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
  table->vkGetImageViewAddressNVX =
      (PFN_vkGetImageViewAddressNVX)load(context, "vkGetImageViewAddressNVX");
  table->vkGetImageViewHandleNVX =
      (PFN_vkGetImageViewHandleNVX)load(context, "vkGetImageViewHandleNVX");
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NV_clip_space_w_scaling)
  table->vkCmdSetViewportWScalingNV =
      (PFN_vkCmdSetViewportWScalingNV)load(context, "vkCmdSetViewportWScalingNV");
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_device_diagnostic_checkpoints)
  table->vkCmdSetCheckpointNV = (PFN_vkCmdSetCheckpointNV)load(context, "vkCmdSetCheckpointNV");
  table->vkGetQueueCheckpointDataNV =
      (PFN_vkGetQueueCheckpointDataNV)load(context, "vkGetQueueCheckpointDataNV");
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_generated_commands)
  table->vkCmdBindPipelineShaderGroupNV =
      (PFN_vkCmdBindPipelineShaderGroupNV)load(context, "vkCmdBindPipelineShaderGroupNV");
  table->vkCmdExecuteGeneratedCommandsNV =
      (PFN_vkCmdExecuteGeneratedCommandsNV)load(context, "vkCmdExecuteGeneratedCommandsNV");
  table->vkCmdPreprocessGeneratedCommandsNV =
      (PFN_vkCmdPreprocessGeneratedCommandsNV)load(context, "vkCmdPreprocessGeneratedCommandsNV");
  table->vkCreateIndirectCommandsLayoutNV =
      (PFN_vkCreateIndirectCommandsLayoutNV)load(context, "vkCreateIndirectCommandsLayoutNV");
  table->vkDestroyIndirectCommandsLayoutNV =
      (PFN_vkDestroyIndirectCommandsLayoutNV)load(context, "vkDestroyIndirectCommandsLayoutNV");
  table->vkGetGeneratedCommandsMemoryRequirementsNV =
      (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)load(
          context, "vkGetGeneratedCommandsMemoryRequirementsNV");
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_external_memory_rdma)
  table->vkGetMemoryRemoteAddressNV =
      (PFN_vkGetMemoryRemoteAddressNV)load(context, "vkGetMemoryRemoteAddressNV");
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
  table->vkGetMemoryWin32HandleNV =
      (PFN_vkGetMemoryWin32HandleNV)load(context, "vkGetMemoryWin32HandleNV");
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
  table->vkCmdSetFragmentShadingRateEnumNV =
      (PFN_vkCmdSetFragmentShadingRateEnumNV)load(context, "vkCmdSetFragmentShadingRateEnumNV");
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_mesh_shader)
  table->vkCmdDrawMeshTasksIndirectCountNV =
      (PFN_vkCmdDrawMeshTasksIndirectCountNV)load(context, "vkCmdDrawMeshTasksIndirectCountNV");
  table->vkCmdDrawMeshTasksIndirectNV =
      (PFN_vkCmdDrawMeshTasksIndirectNV)load(context, "vkCmdDrawMeshTasksIndirectNV");
  table->vkCmdDrawMeshTasksNV = (PFN_vkCmdDrawMeshTasksNV)load(context, "vkCmdDrawMeshTasksNV");
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_ray_tracing)
  table->vkBindAccelerationStructureMemoryNV =
      (PFN_vkBindAccelerationStructureMemoryNV)load(context, "vkBindAccelerationStructureMemoryNV");
  table->vkCmdBuildAccelerationStructureNV =
      (PFN_vkCmdBuildAccelerationStructureNV)load(context, "vkCmdBuildAccelerationStructureNV");
  table->vkCmdCopyAccelerationStructureNV =
      (PFN_vkCmdCopyAccelerationStructureNV)load(context, "vkCmdCopyAccelerationStructureNV");
  table->vkCmdTraceRaysNV = (PFN_vkCmdTraceRaysNV)load(context, "vkCmdTraceRaysNV");
  table->vkCmdWriteAccelerationStructuresPropertiesNV =
      (PFN_vkCmdWriteAccelerationStructuresPropertiesNV)load(
          context, "vkCmdWriteAccelerationStructuresPropertiesNV");
  table->vkCompileDeferredNV = (PFN_vkCompileDeferredNV)load(context, "vkCompileDeferredNV");
  table->vkCreateAccelerationStructureNV =
      (PFN_vkCreateAccelerationStructureNV)load(context, "vkCreateAccelerationStructureNV");
  table->vkCreateRayTracingPipelinesNV =
      (PFN_vkCreateRayTracingPipelinesNV)load(context, "vkCreateRayTracingPipelinesNV");
  table->vkDestroyAccelerationStructureNV =
      (PFN_vkDestroyAccelerationStructureNV)load(context, "vkDestroyAccelerationStructureNV");
  table->vkGetAccelerationStructureHandleNV =
      (PFN_vkGetAccelerationStructureHandleNV)load(context, "vkGetAccelerationStructureHandleNV");
  table->vkGetAccelerationStructureMemoryRequirementsNV =
      (PFN_vkGetAccelerationStructureMemoryRequirementsNV)load(
          context, "vkGetAccelerationStructureMemoryRequirementsNV");
  table->vkGetRayTracingShaderGroupHandlesNV =
      (PFN_vkGetRayTracingShaderGroupHandlesNV)load(context, "vkGetRayTracingShaderGroupHandlesNV");
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive)
  table->vkCmdSetExclusiveScissorNV =
      (PFN_vkCmdSetExclusiveScissorNV)load(context, "vkCmdSetExclusiveScissorNV");
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
  table->vkCmdBindShadingRateImageNV =
      (PFN_vkCmdBindShadingRateImageNV)load(context, "vkCmdBindShadingRateImageNV");
  table->vkCmdSetCoarseSampleOrderNV =
      (PFN_vkCmdSetCoarseSampleOrderNV)load(context, "vkCmdSetCoarseSampleOrderNV");
  table->vkCmdSetViewportShadingRatePaletteNV = (PFN_vkCmdSetViewportShadingRatePaletteNV)load(
      context, "vkCmdSetViewportShadingRatePaletteNV");
#endif /* defined(VK_NV_shading_rate_image) */
#if (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || \
    (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1))
  table->vkGetDeviceGroupSurfacePresentModes2EXT =
      (PFN_vkGetDeviceGroupSurfacePresentModes2EXT)load(context,
                                                        "vkGetDeviceGroupSurfacePresentModes2EXT");
#endif /* (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || \
          (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || \
    (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                    \
    (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template))
  table->vkCmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)load(
      context, "vkCmdPushDescriptorSetWithTemplateKHR");
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || \
          (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                    \
          (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  table->vkGetDeviceGroupPresentCapabilitiesKHR = (PFN_vkGetDeviceGroupPresentCapabilitiesKHR)load(
      context, "vkGetDeviceGroupPresentCapabilitiesKHR");
  table->vkGetDeviceGroupSurfacePresentModesKHR = (PFN_vkGetDeviceGroupSurfacePresentModesKHR)load(
      context, "vkGetDeviceGroupSurfacePresentModesKHR");
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) \
          && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  table->vkAcquireNextImage2KHR =
      (PFN_vkAcquireNextImage2KHR)load(context, "vkAcquireNextImage2KHR");
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || \
          (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
  /* IGL_GENERATE_LOAD_DEVICE_TABLE */
}

#ifdef __cplusplus
}
#endif
