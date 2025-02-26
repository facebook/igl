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

#if defined(IGL_CMAKE_BUILD)
#include <volk.h>
#else
#if defined(VK_USE_PLATFORM_WIN32_KHR)

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

/* When VK_USE_PLATFORM_WIN32_KHR is defined, instead of including vulkan.h directly, we include
 * individual parts of the SDK This is necessary to avoid including <windows.h> which is very
 * heavy
 * - it takes 200ms to parse without WIN32_LEAN_AND_MEAN and 100ms to parse with it.
 * vulkan_win32.h only needs a few symbols that are easy to redefine ourselves.
 */
typedef unsigned long DWORD;
typedef const wchar_t* LPCWSTR;
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HWND__* HWND;
typedef struct HMONITOR__* HMONITOR;
typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES;

#include <vulkan/vulkan_win32.h>

#ifdef VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan_beta.h>
#endif
#else
#include <vulkan/vulkan.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Vulkan function table. On some systems, multiple Vulkan loaders can be executed, which
 * may cause a different set of functions to be loaded or unloaded after a Vulkan context has been
 * created. This structure stores one set of functions that can be used to call Vulkan functions
 * for one session. It is populated by the `loadVulkanLoaderFunctions()` function declared in this
 * file.
 */
struct VulkanFunctionTable {
  /* IGL_GENERATE_FUNCTION_TABLE */
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
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkCreateEvent vkCreateEvent;
  PFN_vkCreateFence vkCreateFence;
  PFN_vkCreateFramebuffer vkCreateFramebuffer;
  PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
  PFN_vkCreateImage vkCreateImage;
  PFN_vkCreateImageView vkCreateImageView;
  PFN_vkCreateInstance vkCreateInstance;
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
  PFN_vkDestroyInstance vkDestroyInstance;
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
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
  PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
  PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
  PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
  PFN_vkFreeMemory vkFreeMemory;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;
  PFN_vkGetEventStatus vkGetEventStatus;
  PFN_vkGetFenceStatus vkGetFenceStatus;
  PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
  PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
  PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
  PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
  PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
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
#else
  PFN_vkVoidFunction __ignore_alignment1[137];
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
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
  PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
  PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
  PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
  PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
  PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
  PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
  PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
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
  PFN_vkTrimCommandPool vkTrimCommandPool;
  PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#else
  PFN_vkVoidFunction __ignore_alignment2[28];
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
#else
  PFN_vkVoidFunction __ignore_alignment3[13];
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
  PFN_vkGetPhysicalDeviceToolProperties vkGetPhysicalDeviceToolProperties;
  PFN_vkGetPrivateData vkGetPrivateData;
  PFN_vkQueueSubmit2 vkQueueSubmit2;
  PFN_vkSetPrivateData vkSetPrivateData;
#else
  PFN_vkVoidFunction __ignore_alignment4[37];
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_AMD_buffer_marker)
  PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#else
  PFN_vkVoidFunction __ignore_alignment5;
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_display_native_hdr)
  PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#else
  PFN_vkVoidFunction __ignore_alignment6;
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
  PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
  PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#else
  PFN_vkVoidFunction __ignore_alignment7[2];
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
  PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#else
  PFN_vkVoidFunction __ignore_alignment8;
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
  PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
  PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#else
  PFN_vkVoidFunction _ignore_alignment9[2];
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_EXT_acquire_drm_display)
  PFN_vkAcquireDrmDisplayEXT vkAcquireDrmDisplayEXT;
  PFN_vkGetDrmDisplayEXT vkGetDrmDisplayEXT;
#else
  PFN_vkVoidFunction __ignore_alignment10[2];
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
  PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
  PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#else
  PFN_vkVoidFunction _ignore_alignment11[2];
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_buffer_device_address)
  PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#else
  PFN_vkVoidFunction __ignore_alignment12;
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
  PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
  PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
#else
  PFN_vkVoidFunction __ignore_alignment13[2];
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
  PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
#else
  PFN_vkVoidFunction __ignore_alignment14;
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
  PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
  PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#else
  PFN_vkVoidFunction __ignore_alignment15[2];
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
  PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#else
  PFN_vkVoidFunction __ignore_alignment16[5];
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_debug_report)
  PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
  PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
  PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
#else
  PFN_vkVoidFunction __ignore_alignment17[3];
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
#else
  PFN_vkVoidFunction __ignore_alignment18[11];
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_direct_mode_display)
  PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#else
  PFN_vkVoidFunction __ignore_alignment19;
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
  PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
  PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT
      vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#else
  PFN_vkVoidFunction _ignore_alignment20[2];
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_discard_rectangles)
  PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#else
  PFN_vkVoidFunction __ignore_alignment21;
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_display_control)
  PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
  PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
  PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
  PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#else
  PFN_vkVoidFunction __ignore_alignment22[4];
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_display_surface_counter)
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#else
  PFN_vkVoidFunction __ignore_alignment23;
#endif /* defined(VK_EXT_display_surface_counter) */
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
#else
  PFN_vkVoidFunction __ignore_alignment24[12];
#endif /* defined(VK_EXT_extended_dynamic_state) */
#if defined(VK_EXT_extended_dynamic_state2)
  PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
  PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
  PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
  PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
  PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
#else
  PFN_vkVoidFunction __ignore_alignment25[5];
#endif /* defined(VK_EXT_extended_dynamic_state2) */
#if defined(VK_EXT_external_memory_host)
  PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#else
  PFN_vkVoidFunction __ignore_alignment26;
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_full_screen_exclusive)
  PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
  PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
  PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#else
  PFN_vkVoidFunction _ignore_alignment27[3];
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_hdr_metadata)
  PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#else
  PFN_vkVoidFunction __ignore_alignment28;
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_headless_surface)
  PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#else
  PFN_vkVoidFunction __ignore_alignment29;
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_host_query_reset)
  PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#else
  PFN_vkVoidFunction __ignore_alignment30;
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
  PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#else
  PFN_vkVoidFunction __ignore_alignment31;
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
  PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#else
  PFN_vkVoidFunction __ignore_alignment32;
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_metal_surface)
  PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#else
  PFN_vkVoidFunction _ignore_alignment33;
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_multi_draw)
  PFN_vkCmdDrawMultiEXT vkCmdDrawMultiEXT;
  PFN_vkCmdDrawMultiIndexedEXT vkCmdDrawMultiIndexedEXT;
#else
  PFN_vkVoidFunction __ignore_alignment34[2];
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_pageable_device_local_memory)
  PFN_vkSetDeviceMemoryPriorityEXT vkSetDeviceMemoryPriorityEXT;
#else
  PFN_vkVoidFunction __ignore_alignment35;
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_private_data)
  PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
  PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
  PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
  PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
#else
  PFN_vkVoidFunction __ignore_alignment36[4];
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
  PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
  PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#else
  PFN_vkVoidFunction __ignore_alignment37[2];
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_tooling_info)
  PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
#else
  PFN_vkVoidFunction __ignore_alignment38;
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_EXT_transform_feedback)
  PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
  PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
  PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
  PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
  PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
  PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#else
  PFN_vkVoidFunction __ignore_alignment39[6];
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
  PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
  PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
  PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
  PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#else
  PFN_vkVoidFunction __ignore_alignment40[4];
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_EXT_vertex_input_dynamic_state)
  PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
#else
  PFN_vkVoidFunction __ignore_alignment41;
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */
#if defined(VK_FUCHSIA_buffer_collection)
  PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIA;
  PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIA;
  PFN_vkGetBufferCollectionPropertiesFUCHSIA vkGetBufferCollectionPropertiesFUCHSIA;
  PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA vkSetBufferCollectionBufferConstraintsFUCHSIA;
  PFN_vkSetBufferCollectionImageConstraintsFUCHSIA vkSetBufferCollectionImageConstraintsFUCHSIA;
#else
  PFN_vkVoidFunction _ignore_alignment42[5];
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
  PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
  PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#else
  PFN_vkVoidFunction _ignore_alignment43[2];
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
  PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
  PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
#else
  PFN_vkVoidFunction _ignore_alignment44[2];
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_FUCHSIA_imagepipe_surface)
  PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#else
  PFN_vkVoidFunction _ignore_alignment45;
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
  PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#else
  PFN_vkVoidFunction _ignore_alignment46;
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_GOOGLE_display_timing)
  PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
  PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#else
  PFN_vkVoidFunction __ignore_alignment47[2];
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_invocation_mask)
  PFN_vkCmdBindInvocationMaskHUAWEI vkCmdBindInvocationMaskHUAWEI;
#else
  PFN_vkVoidFunction __ignore_alignment48;
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading)
  PFN_vkCmdSubpassShadingHUAWEI vkCmdSubpassShadingHUAWEI;
  PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI
      vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
#else
  PFN_vkVoidFunction __ignore_alignment49[2];
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
#else
  PFN_vkVoidFunction __ignore_alignment50[9];
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
#else
  PFN_vkVoidFunction __ignore_alignment51[16];
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_android_surface)
  PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#else
  PFN_vkVoidFunction _ignore_alignment52;
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_bind_memory2)
  PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
  PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment53[2];
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
  PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
  PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
  PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#else
  PFN_vkVoidFunction __ignore_alignment54[3];
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_copy_commands2)
  PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
  PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
  PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
  PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
  PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
  PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment55[6];
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
  PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
  PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
  PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
  PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment56[4];
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
  PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
  PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
  PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
  PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
  PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
#else
  PFN_vkVoidFunction __ignore_alignment57[5];
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
  PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
  PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
  PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#else
  PFN_vkVoidFunction __ignore_alignment58[3];
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
  PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
  PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
  PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment59[3];
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_device_group_creation)
  PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#else
  PFN_vkVoidFunction __ignore_alignment60;
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
  PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
  PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
  PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
  PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
  PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
  PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
  PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment61[7];
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_display_swapchain)
  PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#else
  PFN_vkVoidFunction __ignore_alignment62;
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
  PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
  PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#else
  PFN_vkVoidFunction __ignore_alignment63[2];
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
  PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
  PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
#else
  PFN_vkVoidFunction __ignore_alignment64[2];
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_external_fence_capabilities)
  PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment65;
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_fence_fd)
  PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
  PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#else
  PFN_vkVoidFunction __ignore_alignment66[2];
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
  PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
  PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#else
  PFN_vkVoidFunction _ignore_alignment67[2];
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_capabilities)
  PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment68;
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_memory_fd)
  PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
  PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment69[2];
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
  PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
  PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#else
  PFN_vkVoidFunction _ignore_alignment70[2];
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_capabilities)
  PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR
      vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment71;
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_external_semaphore_fd)
  PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
  PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#else
  PFN_vkVoidFunction __ignore_alignment72[2];
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
  PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
  PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#else
  PFN_vkVoidFunction _ignore_alignment73[2];
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
  PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
  PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment74[2];
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
  PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
  PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
  PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
  PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment75[4];
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_memory_requirements2)
  PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
  PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
  PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment76[3];
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_get_physical_device_properties2)
  PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
  PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
  PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
  PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
  PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR
      vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment77[7];
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
  PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment78[2];
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_maintenance1)
  PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#else
  PFN_vkVoidFunction __ignore_alignment79;
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
  PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#else
  PFN_vkVoidFunction __ignore_alignment80;
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
  PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR;
  PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR;
  PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR;
#else
  PFN_vkVoidFunction __ignore_alignment81[3];
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_performance_query)
  PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
  PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR
      vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
  PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR
      vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
  PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#else
  PFN_vkVoidFunction __ignore_alignment82[4];
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_executable_properties)
  PFN_vkGetPipelineExecutableInternalRepresentationsKHR
      vkGetPipelineExecutableInternalRepresentationsKHR;
  PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
  PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#else
  PFN_vkVoidFunction __ignore_alignment83[3];
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
  PFN_vkWaitForPresentKHR vkWaitForPresentKHR;
#else
  PFN_vkVoidFunction __ignore_alignment84;
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_push_descriptor)
  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#else
  PFN_vkVoidFunction __ignore_alignment85;
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
#else
  PFN_vkVoidFunction __ignore_alignment86[7];
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
  PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
  PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#else
  PFN_vkVoidFunction __ignore_alignment87[2];
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
  PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#else
  PFN_vkVoidFunction __ignore_alignment88;
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_surface)
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
#else
  PFN_vkVoidFunction __ignore_alignment89[5];
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_swapchain)
  PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
  PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
  PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
  PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
  PFN_vkQueuePresentKHR vkQueuePresentKHR;
#else
  PFN_vkVoidFunction __ignore_alignment90[5];
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_synchronization2)
  PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
  PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
  PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
  PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
  PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
  PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment91[6];
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker)
  PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
#else
  PFN_vkVoidFunction __ignore_alignment92;
#endif /* defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker) */
#if defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints)
  PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
#else
  PFN_vkVoidFunction __ignore_alignment93;
#endif /* defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_KHR_timeline_semaphore)
  PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
  PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
  PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#else
  PFN_vkVoidFunction __ignore_alignment94[3];
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
  PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#else
  PFN_vkVoidFunction _ignore_alignment95;
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
  PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
#else
  PFN_vkVoidFunction _ignore_alignment96;
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
  PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
  PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
  PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
#else
  PFN_vkVoidFunction _ignore_alignment97[12];
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
  PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
  PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR
      vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#else
  PFN_vkVoidFunction _ignore_alignment98[2];
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
  PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#else
  PFN_vkVoidFunction _ignore_alignment99[2];
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
  PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
  PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#else
  PFN_vkVoidFunction _ignore_alignment100[2];
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
  PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
  PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#else
  PFN_vkVoidFunction _ignore_alignment101[2];
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
  PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#else
  PFN_vkVoidFunction _ignore_alignment102;
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
  PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#else
  PFN_vkVoidFunction _ignore_alignment103;
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
  PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#else
  PFN_vkVoidFunction _ignore_alignment104;
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NVX_binary_import)
  PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
  PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
  PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
  PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
  PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
#else
  PFN_vkVoidFunction __ignore_alignment105[5];
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
  PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
  PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#else
  PFN_vkVoidFunction __ignore_alignment106[2];
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NV_acquire_winrt_display)
  PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
  PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#else
  PFN_vkVoidFunction __ignore_alignment107[2];
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_clip_space_w_scaling)
  PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#else
  PFN_vkVoidFunction __ignore_alignment108;
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_cooperative_matrix)
  PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV
      vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#else
  PFN_vkVoidFunction __ignore_alignment109;
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_coverage_reduction_mode)
  PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV
      vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#else
  PFN_vkVoidFunction __ignore_alignment110;
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_device_diagnostic_checkpoints)
  PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
  PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#else
  PFN_vkVoidFunction __ignore_alignment111[2];
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_generated_commands)
  PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
  PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
  PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
  PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
  PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
  PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
#else
  PFN_vkVoidFunction __ignore_alignment112[6];
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_external_memory_capabilities)
  PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV
      vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#else
  PFN_vkVoidFunction __ignore_alignment113;
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_NV_external_memory_rdma)
  PFN_vkGetMemoryRemoteAddressNV vkGetMemoryRemoteAddressNV;
#else
  PFN_vkVoidFunction __ignore_alignment114;
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
  PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#else
  PFN_vkVoidFunction _ignore_alignment115;
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
  PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#else
  PFN_vkVoidFunction __ignore_alignment116;
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_mesh_shader)
  PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
  PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
  PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#else
  PFN_vkVoidFunction __ignore_alignment117[3];
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
#else
  PFN_vkVoidFunction __ignore_alignment118[12];
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive)
  PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#else
  PFN_vkVoidFunction __ignore_alignment119;
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
  PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
  PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
  PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#else
  PFN_vkVoidFunction __ignore_alignment120[3];
#endif /* defined(VK_NV_shading_rate_image) */
#if defined(VK_QNX_screen_surface)
  PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
  PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX
      vkGetPhysicalDeviceScreenPresentationSupportQNX;
#else
  PFN_vkVoidFunction _ignore_alignment121[2];
#endif /* defined(VK_QNX_screen_surface) */
#if (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || \
    (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1))
  PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#else
  PFN_vkVoidFunction _ignore_alignment122;
#endif /* (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || \
          (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || \
    (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                    \
    (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template))
  PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#else
  PFN_vkVoidFunction __ignore_alignment123;
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || \
          (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                    \
          (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
  PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
  PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#else
  PFN_vkVoidFunction __ignore_alignment124[3];
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) \
          && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
  PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#else
  PFN_vkVoidFunction __ignore_alignment125;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || \
          (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
  /* IGL_GENERATE_FUNCTION_TABLE */
};

#ifdef __cplusplus
/* IGL_GENERATE_SIZE_CHECK */
static_assert(sizeof(VulkanFunctionTable) == 543 * sizeof(PFN_vkVoidFunction));
/* IGL_GENERATE_SIZE_CHECK */
#endif

/// @brief Populates the `VulkanFunctionTable` structure. Requires a pointer to the
/// vkGetInstanceProcAddr function, which is used to retrieve pointers to all non-instance and
/// no-device functions defined in the `VulkanFunctionTable` structure.
int loadVulkanLoaderFunctions(struct VulkanFunctionTable* table, PFN_vkGetInstanceProcAddr load);

/// @brief Populates the instance function pointers in the `VulkanFunctionTable` structure. Requires
/// a pointer to the vkGetInstanceProcAddr function, which is used to retrieve pointers to all
/// instance functions defined in the `VulkanFunctionTable` structure.
void loadVulkanInstanceFunctions(struct VulkanFunctionTable* table,
                                 VkInstance context,
                                 PFN_vkGetInstanceProcAddr load,
                                 VkBool32 enableExtDebugUtils);

/// @brief Populates the device function pointers in the `VulkanFunctionTable` structure. Requires a
/// pointer to the vkGetInstanceProcAddr function, which is used to retrieve pointers to all device
/// functions defined in the `VulkanFunctionTable` structure.
void loadVulkanDeviceFunctions(struct VulkanFunctionTable* table,
                               VkDevice context,
                               PFN_vkGetDeviceProcAddr load);

#ifdef __cplusplus
}
#endif
