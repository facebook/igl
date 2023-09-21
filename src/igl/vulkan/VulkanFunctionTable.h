/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Vulkan functions tables, supplementary for volk.

#pragma once

#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif // !defined(VK_NO_PROTOTYPES)

#include <volk.h>

#ifdef __cplusplus
extern "C" {
#endif

struct VulkanFunctionTable {
  /* IGL_GENERATE_LOADER_TABLE */
#if defined(VK_VERSION_1_0)
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
#endif /* defined(VK_VERSION_1_1) */
  /* IGL_GENERATE_LOADER_TABLE */
  /* IGL_GENERATE_INSTANCE_TABLE */
#if defined(VK_VERSION_1_0)
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkDestroyInstance vkDestroyInstance;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
  PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
  PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
  PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
  PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
  PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
  PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
  PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
  PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
  PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
  PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
  PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
  PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties2
      vkGetPhysicalDeviceSparseImageFormatProperties2;
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_3)
  PFN_vkGetPhysicalDeviceToolProperties vkGetPhysicalDeviceToolProperties;
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_EXT_acquire_drm_display)
  PFN_vkAcquireDrmDisplayEXT vkAcquireDrmDisplayEXT;
  PFN_vkGetDrmDisplayEXT vkGetDrmDisplayEXT;
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
  PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
  PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_calibrated_timestamps)
  PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_debug_report)
  PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
  PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
  PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
  PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
  PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
  PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
  PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
  PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
  PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
  PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
  PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_direct_mode_display)
  PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
  PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
  PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT
      vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_display_surface_counter)
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_full_screen_exclusive)
  PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_headless_surface)
  PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_metal_surface)
  PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_sample_locations)
  PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_tooling_info)
  PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_FUCHSIA_imagepipe_surface)
  PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
  PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_KHR_android_surface)
  PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_device_group_creation)
  PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
  PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
  PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
  PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
  PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
  PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
  PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
  PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_external_fence_capabilities)
  PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_memory_capabilities)
  PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_semaphore_capabilities)
  PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR
      vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_fragment_shading_rate)
  PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
  PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
  PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
  PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
  PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_physical_device_properties2)
  PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
  PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
  PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
  PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
  PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR
      vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
  PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_performance_query)
  PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR
      vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
  PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR
      vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_surface)
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_video_queue)
  PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
  PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
  PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR
      vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
  PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
  PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
  PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
  PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
  PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
  PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
  PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
  PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NV_acquire_winrt_display)
  PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
  PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_cooperative_matrix)
  PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV
      vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_coverage_reduction_mode)
  PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV
      vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_external_memory_capabilities)
  PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV
      vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_QNX_screen_surface)
  PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
  PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX
      vkGetPhysicalDeviceScreenPresentationSupportQNX;
#endif /* defined(VK_QNX_screen_surface) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) \
          && defined(VK_VERSION_1_1)) */
  /* IGL_GENERATE_INSTANCE_TABLE */
  /* IGL_GENERATE_DEVICE_TABLE */
#if defined(VK_VERSION_1_0)
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
  PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
  PFN_vkAllocateMemory vkAllocateMemory;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
  PFN_vkBindBufferMemory vkBindBufferMemory;
  PFN_vkBindImageMemory vkBindImageMemory;
  PFN_vkCmdBeginQuery vkCmdBeginQuery;
  PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
  PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
  PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
  PFN_vkCmdBindPipeline vkCmdBindPipeline;
  PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
  PFN_vkCmdBlitImage vkCmdBlitImage;
  PFN_vkCmdClearAttachments vkCmdClearAttachments;
  PFN_vkCmdClearColorImage vkCmdClearColorImage;
  PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
  PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
  PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
  PFN_vkCmdCopyImage vkCmdCopyImage;
  PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
  PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
  PFN_vkCmdDispatch vkCmdDispatch;
  PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
  PFN_vkCmdDraw vkCmdDraw;
  PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
  PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
  PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
  PFN_vkCmdEndQuery vkCmdEndQuery;
  PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
  PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
  PFN_vkCmdFillBuffer vkCmdFillBuffer;
  PFN_vkCmdNextSubpass vkCmdNextSubpass;
  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
  PFN_vkCmdPushConstants vkCmdPushConstants;
  PFN_vkCmdResetEvent vkCmdResetEvent;
  PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
  PFN_vkCmdResolveImage vkCmdResolveImage;
  PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
  PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
  PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
  PFN_vkCmdSetEvent vkCmdSetEvent;
  PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
  PFN_vkCmdSetScissor vkCmdSetScissor;
  PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
  PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
  PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
  PFN_vkCmdSetViewport vkCmdSetViewport;
  PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
  PFN_vkCmdWaitEvents vkCmdWaitEvents;
  PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
  PFN_vkCreateBuffer vkCreateBuffer;
  PFN_vkCreateBufferView vkCreateBufferView;
  PFN_vkCreateCommandPool vkCreateCommandPool;
  PFN_vkCreateComputePipelines vkCreateComputePipelines;
  PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
  PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
  PFN_vkCreateEvent vkCreateEvent;
  PFN_vkCreateFence vkCreateFence;
  PFN_vkCreateFramebuffer vkCreateFramebuffer;
  PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
  PFN_vkCreateImage vkCreateImage;
  PFN_vkCreateImageView vkCreateImageView;
  PFN_vkCreatePipelineCache vkCreatePipelineCache;
  PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
  PFN_vkCreateQueryPool vkCreateQueryPool;
  PFN_vkCreateRenderPass vkCreateRenderPass;
  PFN_vkCreateSampler vkCreateSampler;
  PFN_vkCreateSemaphore vkCreateSemaphore;
  PFN_vkCreateShaderModule vkCreateShaderModule;
  PFN_vkDestroyBuffer vkDestroyBuffer;
  PFN_vkDestroyBufferView vkDestroyBufferView;
  PFN_vkDestroyCommandPool vkDestroyCommandPool;
  PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
  PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
  PFN_vkDestroyDevice vkDestroyDevice;
  PFN_vkDestroyEvent vkDestroyEvent;
  PFN_vkDestroyFence vkDestroyFence;
  PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
  PFN_vkDestroyImage vkDestroyImage;
  PFN_vkDestroyImageView vkDestroyImageView;
  PFN_vkDestroyPipeline vkDestroyPipeline;
  PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
  PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
  PFN_vkDestroyQueryPool vkDestroyQueryPool;
  PFN_vkDestroyRenderPass vkDestroyRenderPass;
  PFN_vkDestroySampler vkDestroySampler;
  PFN_vkDestroySemaphore vkDestroySemaphore;
  PFN_vkDestroyShaderModule vkDestroyShaderModule;
  PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
  PFN_vkEndCommandBuffer vkEndCommandBuffer;
  PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
  PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
  PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
  PFN_vkFreeMemory vkFreeMemory;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;
  PFN_vkGetEventStatus vkGetEventStatus;
  PFN_vkGetFenceStatus vkGetFenceStatus;
  PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
  PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
  PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
  PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
  PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
  PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
  PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
  PFN_vkMapMemory vkMapMemory;
  PFN_vkMergePipelineCaches vkMergePipelineCaches;
  PFN_vkQueueBindSparse vkQueueBindSparse;
  PFN_vkQueueSubmit vkQueueSubmit;
  PFN_vkQueueWaitIdle vkQueueWaitIdle;
  PFN_vkResetCommandBuffer vkResetCommandBuffer;
  PFN_vkResetCommandPool vkResetCommandPool;
  PFN_vkResetDescriptorPool vkResetDescriptorPool;
  PFN_vkResetEvent vkResetEvent;
  PFN_vkResetFences vkResetFences;
  PFN_vkSetEvent vkSetEvent;
  PFN_vkUnmapMemory vkUnmapMemory;
  PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
  PFN_vkWaitForFences vkWaitForFences;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
  PFN_vkBindBufferMemory2 vkBindBufferMemory2;
  PFN_vkBindImageMemory2 vkBindImageMemory2;
  PFN_vkCmdDispatchBase vkCmdDispatchBase;
  PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
  PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
  PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
  PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
  PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
  PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
  PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
  PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
  PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
  PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
  PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
  PFN_vkTrimCommandPool vkTrimCommandPool;
  PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
  PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2;
  PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount;
  PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount;
  PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2;
  PFN_vkCmdNextSubpass2 vkCmdNextSubpass2;
  PFN_vkCreateRenderPass2 vkCreateRenderPass2;
  PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
  PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress;
  PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress;
  PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
  PFN_vkResetQueryPool vkResetQueryPool;
  PFN_vkSignalSemaphore vkSignalSemaphore;
  PFN_vkWaitSemaphores vkWaitSemaphores;
#endif /* defined(VK_VERSION_1_2) */
#if defined(VK_VERSION_1_3)
  PFN_vkCmdBeginRendering vkCmdBeginRendering;
  PFN_vkCmdBindVertexBuffers2 vkCmdBindVertexBuffers2;
  PFN_vkCmdBlitImage2 vkCmdBlitImage2;
  PFN_vkCmdCopyBuffer2 vkCmdCopyBuffer2;
  PFN_vkCmdCopyBufferToImage2 vkCmdCopyBufferToImage2;
  PFN_vkCmdCopyImage2 vkCmdCopyImage2;
  PFN_vkCmdCopyImageToBuffer2 vkCmdCopyImageToBuffer2;
  PFN_vkCmdEndRendering vkCmdEndRendering;
  PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2;
  PFN_vkCmdResetEvent2 vkCmdResetEvent2;
  PFN_vkCmdResolveImage2 vkCmdResolveImage2;
  PFN_vkCmdSetCullMode vkCmdSetCullMode;
  PFN_vkCmdSetDepthBiasEnable vkCmdSetDepthBiasEnable;
  PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable;
  PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp;
  PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable;
  PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable;
  PFN_vkCmdSetEvent2 vkCmdSetEvent2;
  PFN_vkCmdSetFrontFace vkCmdSetFrontFace;
  PFN_vkCmdSetPrimitiveRestartEnable vkCmdSetPrimitiveRestartEnable;
  PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology;
  PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnable;
  PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount;
  PFN_vkCmdSetStencilOp vkCmdSetStencilOp;
  PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable;
  PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount;
  PFN_vkCmdWaitEvents2 vkCmdWaitEvents2;
  PFN_vkCmdWriteTimestamp2 vkCmdWriteTimestamp2;
  PFN_vkCreatePrivateDataSlot vkCreatePrivateDataSlot;
  PFN_vkDestroyPrivateDataSlot vkDestroyPrivateDataSlot;
  PFN_vkGetDeviceBufferMemoryRequirements vkGetDeviceBufferMemoryRequirements;
  PFN_vkGetDeviceImageMemoryRequirements vkGetDeviceImageMemoryRequirements;
  PFN_vkGetDeviceImageSparseMemoryRequirements vkGetDeviceImageSparseMemoryRequirements;
  PFN_vkGetPrivateData vkGetPrivateData;
  PFN_vkQueueSubmit2 vkQueueSubmit2;
  PFN_vkSetPrivateData vkSetPrivateData;
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_AMD_buffer_marker)
  PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_display_native_hdr)
  PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
  PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
  PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
  PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
  PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
  PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_EXT_buffer_device_address)
  PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
  PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
  PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
  PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
  PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
  PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_discard_rectangles)
  PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_display_control)
  PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
  PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
  PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
  PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_extended_dynamic_state)
  PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
  PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
  PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
  PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
  PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
  PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
  PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
  PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
  PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
  PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
  PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
  PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;
#endif /* defined(VK_EXT_extended_dynamic_state) */
#if defined(VK_EXT_extended_dynamic_state2)
  PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
  PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
  PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
  PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
  PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
#endif /* defined(VK_EXT_extended_dynamic_state2) */
#if defined(VK_EXT_external_memory_host)
  PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_full_screen_exclusive)
  PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
  PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_hdr_metadata)
  PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_host_query_reset)
  PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
  PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
  PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_multi_draw)
  PFN_vkCmdDrawMultiEXT vkCmdDrawMultiEXT;
  PFN_vkCmdDrawMultiIndexedEXT vkCmdDrawMultiIndexedEXT;
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_pageable_device_local_memory)
  PFN_vkSetDeviceMemoryPriorityEXT vkSetDeviceMemoryPriorityEXT;
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_private_data)
  PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
  PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
  PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
  PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
  PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_transform_feedback)
  PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
  PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
  PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
  PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
  PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
  PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
  PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
  PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
  PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
  PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_EXT_vertex_input_dynamic_state)
  PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */
#if defined(VK_FUCHSIA_buffer_collection)
  PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIA;
  PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIA;
  PFN_vkGetBufferCollectionPropertiesFUCHSIA vkGetBufferCollectionPropertiesFUCHSIA;
  PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA vkSetBufferCollectionBufferConstraintsFUCHSIA;
  PFN_vkSetBufferCollectionImageConstraintsFUCHSIA vkSetBufferCollectionImageConstraintsFUCHSIA;
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
  PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
  PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
  PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
  PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_GOOGLE_display_timing)
  PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
  PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_invocation_mask)
  PFN_vkCmdBindInvocationMaskHUAWEI vkCmdBindInvocationMaskHUAWEI;
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading)
  PFN_vkCmdSubpassShadingHUAWEI vkCmdSubpassShadingHUAWEI;
  PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI
      vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
#endif /* defined(VK_HUAWEI_subpass_shading) */
#if defined(VK_INTEL_performance_query)
  PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
  PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
  PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
  PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
  PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
  PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
  PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
  PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
  PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_acceleration_structure)
  PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
  PFN_vkCmdBuildAccelerationStructuresIndirectKHR vkCmdBuildAccelerationStructuresIndirectKHR;
  PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
  PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
  PFN_vkCmdCopyAccelerationStructureToMemoryKHR vkCmdCopyAccelerationStructureToMemoryKHR;
  PFN_vkCmdCopyMemoryToAccelerationStructureKHR vkCmdCopyMemoryToAccelerationStructureKHR;
  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
  PFN_vkCopyAccelerationStructureKHR vkCopyAccelerationStructureKHR;
  PFN_vkCopyAccelerationStructureToMemoryKHR vkCopyAccelerationStructureToMemoryKHR;
  PFN_vkCopyMemoryToAccelerationStructureKHR vkCopyMemoryToAccelerationStructureKHR;
  PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
  PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
  PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
  PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
  PFN_vkGetDeviceAccelerationStructureCompatibilityKHR
      vkGetDeviceAccelerationStructureCompatibilityKHR;
  PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_bind_memory2)
  PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
  PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
  PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
  PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
  PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_copy_commands2)
  PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
  PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
  PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
  PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
  PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
  PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
  PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
  PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
  PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
  PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
  PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
  PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
  PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
  PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
  PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
  PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
  PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
  PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
  PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
  PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
  PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_display_swapchain)
  PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
  PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
  PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
  PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
  PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_external_fence_fd)
  PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
  PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
  PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
  PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_fd)
  PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
  PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
  PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
  PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_fd)
  PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
  PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
  PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
  PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
  PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_memory_requirements2)
  PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
  PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
  PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_maintenance1)
  PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
  PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
  PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR;
  PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR;
  PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR;
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_performance_query)
  PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
  PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_executable_properties)
  PFN_vkGetPipelineExecutableInternalRepresentationsKHR
      vkGetPipelineExecutableInternalRepresentationsKHR;
  PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
  PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
  PFN_vkWaitForPresentKHR vkWaitForPresentKHR;
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_push_descriptor)
  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_ray_tracing_pipeline)
  PFN_vkCmdSetRayTracingPipelineStackSizeKHR vkCmdSetRayTracingPipelineStackSizeKHR;
  PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR;
  PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
  PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
  PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR
      vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
  PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
  PFN_vkGetRayTracingShaderGroupStackSizeKHR vkGetRayTracingShaderGroupStackSizeKHR;
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
  PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
  PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
  PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_swapchain)
  PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
  PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
  PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
  PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
  PFN_vkQueuePresentKHR vkQueuePresentKHR;
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_synchronization2)
  PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
  PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
  PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
  PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
  PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
  PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker)
  PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
#endif /* defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker) */
#if defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints)
  PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
#endif /* defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_KHR_timeline_semaphore)
  PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
  PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
  PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
  PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
  PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
  PFN_vkBindVideoSessionMemoryKHR vkBindVideoSessionMemoryKHR;
  PFN_vkCmdBeginVideoCodingKHR vkCmdBeginVideoCodingKHR;
  PFN_vkCmdControlVideoCodingKHR vkCmdControlVideoCodingKHR;
  PFN_vkCmdEndVideoCodingKHR vkCmdEndVideoCodingKHR;
  PFN_vkCreateVideoSessionKHR vkCreateVideoSessionKHR;
  PFN_vkCreateVideoSessionParametersKHR vkCreateVideoSessionParametersKHR;
  PFN_vkDestroyVideoSessionKHR vkDestroyVideoSessionKHR;
  PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParametersKHR;
  PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
  PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_NVX_binary_import)
  PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
  PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
  PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
  PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
  PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
  PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
  PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NV_clip_space_w_scaling)
  PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_device_diagnostic_checkpoints)
  PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
  PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_generated_commands)
  PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
  PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
  PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
  PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
  PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
  PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_external_memory_rdma)
  PFN_vkGetMemoryRemoteAddressNV vkGetMemoryRemoteAddressNV;
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
  PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
  PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_mesh_shader)
  PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
  PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
  PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_ray_tracing)
  PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
  PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
  PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
  PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
  PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
  PFN_vkCompileDeferredNV vkCompileDeferredNV;
  PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
  PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
  PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
  PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
  PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
  PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive)
  PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
  PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
  PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
  PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#endif /* defined(VK_NV_shading_rate_image) */
#if (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || \
    (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1))
  PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif /* (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || \
          (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || \
    (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                    \
    (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template))
  PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || \
          (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                    \
          (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
  PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) \
          && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || \
          (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
  /* IGL_GENERATE_DEVICE_TABLE */
};

void loadVulkanLoaderFunctions(struct VulkanFunctionTable* table, PFN_vkGetInstanceProcAddr load);
void loadVulkanInstanceFunctions(struct VulkanFunctionTable* table,
                                 VkInstance context,
                                 PFN_vkGetInstanceProcAddr load);
void loadVulkanDeviceFunctions(struct VulkanFunctionTable* table,
                               VkDevice context,
                               PFN_vkGetDeviceProcAddr load);

#ifdef __cplusplus
}
#endif
