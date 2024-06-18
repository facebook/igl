/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// This is a very low-level C wrapper of the Vulkan API to make it slightly less painful to use.

#pragma once

#include <glslang/Include/glslang_c_interface.h>

#include <igl/Macros.h>
#include <igl/vulkan/VulkanFunctionTable.h>
#include <igl/vulkan/VulkanVma.h>

#define IGL_ARRAY_NUM_ELEMENTS(x) (sizeof(x) / sizeof((x)[0]))

#ifdef __cplusplus
extern "C" {
#endif

const char* ivkGetVulkanResultString(VkResult result);

/// @brief Creates a Vulkan instance with the given parameters. For platforms other than Android,
/// this function will enable the GPU assisted validation and the synchronization validation layers.
/// The application information is also set up to be "IGL/Vulkan" for application and engine names.
/// Both the application and engine versions are set to 1.0.0
VkResult ivkCreateInstance(const struct VulkanFunctionTable* vt,
                           uint32_t apiVersion,
                           uint32_t enableValidation,
                           uint32_t enableGPUAssistedValidation,
                           uint32_t enableSynchronizationValidation,
                           size_t numExtensions,
                           const char** extensions,
                           VkInstance* outInstance);

/// @brief Creates a Debug Utils Messenger if the VK_EXT_debug_utils extension is available and the
/// platform is not Android or Mac Catalyst. Otherwise the function is defined as a no-op that
/// always returns VK_SUCCESS
VkResult ivkCreateDebugUtilsMessenger(const struct VulkanFunctionTable* vt,
                                      VkInstance instance,
                                      PFN_vkDebugUtilsMessengerCallbackEXT callback,
                                      void* logUserData,
                                      VkDebugUtilsMessengerEXT* outMessenger);

// This function uses VK_EXT_debug_report extension, which is deprecated by VK_EXT_debug_utils.
// However, it is available on some Android devices where VK_EXT_debug_utils is not available.
VkResult ivkCreateDebugReportMessenger(const struct VulkanFunctionTable* vt,
                                       VkInstance instance,
                                       PFN_vkDebugReportCallbackEXT callback,
                                       void* logUserData,
                                       VkDebugReportCallbackEXT* outMessenger);

VkResult ivkCreateSemaphore(const struct VulkanFunctionTable* vt,
                            VkDevice device,
                            bool exportable,
                            VkSemaphore* outSemaphore);

VkResult ivkCreateFence(const struct VulkanFunctionTable* vt,
                        VkDevice device,
                        VkFlags flags,
                        bool exportable,
                        VkFence* outFence);

/** @brief Creates a platform specific VkSurfaceKHR object. The surface creation functions
 * conditionally-compiled and guarded by their respective platform specific extension macros defined
 * by the Vulkan API. The current supported platforms, and their macros, are:
 * - Windows: VK_USE_PLATFORM_WIN32_KHR
 * - Linux: VK_USE_PLATFORM_XLIB_KHR
 * - MacOS: VK_USE_PLATFORM_METAL_EXT
 * - Android: VK_USE_PLATFORM_ANDROID_KHR
 */
VkResult ivkCreateSurface(const struct VulkanFunctionTable* vt,
                          VkInstance instance,
                          void* window,
                          void* display,
                          void* layer,
                          VkSurfaceKHR* outSurface);

/** @brief Creates a Vulkan device from the given physical device. The physical device features
 * (VkPhysicalDeviceFeatures) conditionally enabled, based on the provided supported physical device
 * features, are `dualSrcBlend`, `multiDrawIndirect`, `drawIndirectFirstInstance`, `depthBiasClamp`,
 * `fillModeNonSolid`, and `shaderInt16`.
 * The validation layers enabled are defined in `kDefaultValidationLayers`
 * If descriptor indexing is enabled, then the following descriptor indexing features
 * (VkPhysicalDeviceDescriptorIndexingFeaturesEXT) are enabled:
 * `shaderSampledImageArrayNonUniformIndexing`, `descriptorBindingUniformBufferUpdateAfterBind`,
 * `descriptorBindingSampledImageUpdateAfterBind`, `descriptorBindingStorageImageUpdateAfterBind`,
 * `descriptorBindingStorageBufferUpdateAfterBind`, `descriptorBindingUpdateUnusedWhilePending`,
 * `descriptorBindingPartiallyBound`, `runtimeDescriptorArray`.
 * If 16-bit shader floats are enabled, then the following shader float 16 features are enabled:
 * VkPhysicalDevice16BitStorageFeatures::storageBuffer16BitAccess
 * VkPhysicalDeviceShaderFloat16Int8Features::shaderFloat16
 * If the `VK_KHR_buffer_device_address` extension is available, then
 * VkPhysicalDeviceBufferDeviceAddressFeaturesKHR::bufferDeviceAddress is enabled If multiview is
 * enabled, then VkPhysicalDeviceMultiviewFeatures::multiview is enabled
 */
VkResult ivkCreateDevice(const struct VulkanFunctionTable* vt,
                         VkPhysicalDevice physicalDevice,
                         size_t numQueueCreateInfos,
                         const VkDeviceQueueCreateInfo* queueCreateInfos,
                         size_t numDeviceExtensions,
                         const char** deviceExtensions,
                         VkBool32 enableMultiview,
                         VkBool32 enableShaderFloat16,
                         VkBool32 enableBufferDeviceAddress,
                         VkBool32 enableDescriptorIndexing,
                         VkBool32 enableDrawParameters,
                         VkBool32 enableYcbcr,
                         const VkPhysicalDeviceFeatures* supported,
                         VkDevice* outDevice);

VkResult ivkCreateHeadlessSurface(const struct VulkanFunctionTable* vt,
                                  VkInstance instance,
                                  VkSurfaceKHR* surface);

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
                            VkSwapchainKHR* outSwapchain);

/// @brief Creates a Vulkan Sampler object with default values
VkResult ivkCreateSampler(const struct VulkanFunctionTable* vt,
                          VkDevice device,
                          VkSampler* outSampler);

VkSamplerCreateInfo ivkGetSamplerCreateInfo(VkFilter minFilter,
                                            VkFilter magFilter,
                                            VkSamplerMipmapMode mipmapMode,
                                            VkSamplerAddressMode addressModeU,
                                            VkSamplerAddressMode addressModeV,
                                            VkSamplerAddressMode addressModeW,
                                            float minLod,
                                            float maxLod);

VkSamplerYcbcrConversionCreateInfo ivkGetSamplerYcbcrCreateInfo(VkFormat format);

/// @brief Returns VkImageViewCreateInfo with the R, G, B, and A components mapped to themselves
/// (identity)
VkImageViewCreateInfo ivkGetImageViewCreateInfo(VkImage image,
                                                VkImageViewType type,
                                                VkFormat imageFormat,
                                                VkImageSubresourceRange range);

VkResult ivkCreateFramebuffer(const struct VulkanFunctionTable* vt,
                              VkDevice device,
                              uint32_t width,
                              uint32_t height,
                              VkRenderPass renderPass,
                              size_t numAttachments,
                              const VkImageView* attachments,
                              VkFramebuffer* outFramebuffer);

VkResult ivkCreateCommandPool(const struct VulkanFunctionTable* vt,
                              VkDevice device,
                              VkCommandPoolCreateFlags flags,
                              uint32_t queueFamilyIndex,
                              VkCommandPool* outCommandPool);

VkResult ivkAllocateCommandBuffer(const struct VulkanFunctionTable* vt,
                                  VkDevice device,
                                  VkCommandPool commandPool,
                                  VkCommandBuffer* outCommandBuffer);

VkResult ivkAllocateMemory(const struct VulkanFunctionTable* vt,
                           VkPhysicalDevice physDev,
                           VkDevice device,
                           const VkMemoryRequirements* memRequirements,
                           VkMemoryPropertyFlags props,
                           bool enableBufferDeviceAddress,
                           VkDeviceMemory* outMemory);

VkResult ivkAllocateMemory2(const struct VulkanFunctionTable* vt,
                            VkPhysicalDevice physDev,
                            VkDevice device,
                            const VkMemoryRequirements2* memRequirements,
                            VkMemoryPropertyFlags props,
                            bool enableBufferDeviceAddress,
                            VkDeviceMemory* outMemory);

bool ivkIsHostVisibleSingleHeapMemory(const struct VulkanFunctionTable* vt,
                                      VkPhysicalDevice physDev);

uint32_t ivkFindMemoryType(const struct VulkanFunctionTable* vt,
                           VkPhysicalDevice physDev,
                           uint32_t memoryTypeBits,
                           VkMemoryPropertyFlags flags);

VkResult ivkCreateRenderPass(const struct VulkanFunctionTable* vt,
                             VkDevice device,
                             uint32_t numAttachments,
                             const VkAttachmentDescription* attachments,
                             const VkSubpassDescription* subpass,
                             const VkSubpassDependency* dependency,
                             const VkRenderPassMultiviewCreateInfo* renderPassMultiview,
                             VkRenderPass* outRenderPass);

VkResult ivkCreateShaderModuleFromSPIRV(const struct VulkanFunctionTable* vt,
                                        VkDevice device,
                                        const void* dataSPIRV,
                                        size_t size,
                                        VkShaderModule* outShaderModule);

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
                                   VkPipeline* outPipeline);

VkResult ivkCreateComputePipeline(const struct VulkanFunctionTable* vt,
                                  VkDevice device,
                                  VkPipelineCache pipelineCache,
                                  const VkPipelineShaderStageCreateInfo* shaderStage,
                                  VkPipelineLayout pipelineLayout,
                                  VkPipeline* outPipeline);

VkResult ivkCreateDescriptorSetLayout(const struct VulkanFunctionTable* vt,
                                      VkDevice device,
                                      VkDescriptorSetLayoutCreateFlags flags,
                                      uint32_t numBindings,
                                      const VkDescriptorSetLayoutBinding* bindings,
                                      const VkDescriptorBindingFlags* bindingFlags,
                                      VkDescriptorSetLayout* outLayout);

/// @brief Creates a VkDescriptorSetLayoutBinding structure. The binding specifies that the
/// resource is accessible at the vertex, fragment, and compute shader stages.
VkDescriptorSetLayoutBinding ivkGetDescriptorSetLayoutBinding(uint32_t binding,
                                                              VkDescriptorType descriptorType,
                                                              uint32_t descriptorCount);

/// @brief Creates a VkAttachmentDescription structure with load and store operations for the
/// stencil attachment as "Don't Care"
VkAttachmentDescription ivkGetAttachmentDescription(VkFormat format,
                                                    VkAttachmentLoadOp loadOp,
                                                    VkAttachmentStoreOp storeOp,
                                                    VkImageLayout initialLayout,
                                                    VkImageLayout finalLayout,
                                                    VkSampleCountFlagBits samples);

VkAttachmentReference ivkGetAttachmentReference(uint32_t attachment, VkImageLayout layout);

/// @brief Creates s VkSubpassDescription structure with its pipeline bind point equal to
/// `VK_PIPELINE_BIND_POINT_GRAPHICS`
VkSubpassDescription ivkGetSubpassDescription(uint32_t numColorAttachments,
                                              const VkAttachmentReference* refsColor,
                                              const VkAttachmentReference* refsColorResolve,
                                              const VkAttachmentReference* refDepth);

/// @brief Creates a VkSubpassDependency structure with no dependencies between subpasses
VkSubpassDependency ivkGetSubpassDependency(void);

VkRenderPassMultiviewCreateInfo ivkGetRenderPassMultiviewCreateInfo(
    const uint32_t* viewMask,
    const uint32_t* correlationMask);

VkResult ivkAllocateDescriptorSet(const struct VulkanFunctionTable* vt,
                                  VkDevice device,
                                  VkDescriptorPool pool,
                                  VkDescriptorSetLayout layout,
                                  VkDescriptorSet* outDescriptorSet);

VkResult ivkCreateDescriptorPool(const struct VulkanFunctionTable* vt,
                                 VkDevice device,
                                 VkDescriptorPoolCreateFlags flags,
                                 uint32_t maxDescriptorSets,
                                 uint32_t numPoolSizes,
                                 const VkDescriptorPoolSize* poolSizes,
                                 VkDescriptorPool* outDescriptorPool);

/// @brief Starts recording a command buffer that will be submitted only once (i.e. it cannot be
/// reused)
VkResult ivkBeginCommandBuffer(const struct VulkanFunctionTable* vt, VkCommandBuffer buffer);

VkResult ivkEndCommandBuffer(const struct VulkanFunctionTable* vt, VkCommandBuffer buffer);

/// @brief Creates a VkSubmitInfo structure with an optional semaphore, used to signal when the
/// command buffer for this batch have completed execution
VkSubmitInfo ivkGetSubmitInfo(const VkCommandBuffer* buffer,
                              uint32_t numWaitSemaphores,
                              const VkSemaphore* waitSemaphores,
                              const VkPipelineStageFlags* waitStageMasks,
                              const VkSemaphore* releaseSemaphore);

/// @brief Creates a VkAttachmentDescription2 structure with load and store operations for the
/// stencil attachment as "Don't Care"
VkAttachmentDescription2 ivkGetAttachmentDescriptionColor(VkFormat format,
                                                          VkAttachmentLoadOp loadOp,
                                                          VkAttachmentStoreOp storeOp,
                                                          VkImageLayout initialLayout,
                                                          VkImageLayout finalLayout);

/// @brief Creates a VkAttachmentReference2 structure with its layout set to
/// `Vk_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`
VkAttachmentReference2 ivkGetAttachmentReferenceColor(uint32_t idx);

VkClearValue ivkGetClearDepthStencilValue(float depth, uint32_t stencil);
VkClearValue ivkGetClearColorValue(float r, float g, float b, float a);

VkBufferCreateInfo ivkGetBufferCreateInfo(uint64_t size, VkBufferUsageFlags usage);

/// @brief Creates a VkImageCreateInfo structure with its layout set to `VK_IMAGE_LAYOUT_UNDEFINED`
VkImageCreateInfo ivkGetImageCreateInfo(VkImageType type,
                                        VkFormat imageFormat,
                                        VkImageTiling tiling,
                                        VkImageUsageFlags usage,
                                        VkExtent3D extent,
                                        uint32_t mipLevels,
                                        uint32_t arrayLayers,
                                        VkImageCreateFlags flags,
                                        VkSampleCountFlags samples);

VkPipelineVertexInputStateCreateInfo ivkGetPipelineVertexInputStateCreateInfo_Empty(void);

/// @brief Creates an empty VkPipelineVertexInputStateCreateInfo structure
VkPipelineVertexInputStateCreateInfo ivkGetPipelineVertexInputStateCreateInfo(
    uint32_t vbCount,
    const VkVertexInputBindingDescription* bindings,
    uint32_t vaCount,
    const VkVertexInputAttributeDescription* attributes);

VkPipelineInputAssemblyStateCreateInfo ivkGetPipelineInputAssemblyStateCreateInfo(
    VkPrimitiveTopology topology,
    VkBool32 enablePrimitiveRestart);

VkPipelineDynamicStateCreateInfo ivkGetPipelineDynamicStateCreateInfo(
    uint32_t numDynamicStates,
    const VkDynamicState* dynamicStates);

/// @brief Creates a VkPipelineRasterizationStateCreateInfo structure with default values, except
/// for polygon mode and cull mode flags
VkPipelineRasterizationStateCreateInfo ivkGetPipelineRasterizationStateCreateInfo(
    VkPolygonMode polygonMode,
    VkCullModeFlags cullMode);

/// @brief Creates a VkPipelineMultisampleStateCreateInfo structure with default values
VkPipelineMultisampleStateCreateInfo ivkGetPipelineMultisampleStateCreateInfo_Empty(void);

/// @brief Creates a VkPipelineDepthStencilStateCreateInfo structure with depth test and write
/// disabled
VkPipelineDepthStencilStateCreateInfo ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests(
    void);

/// @brief Creates a VkPipelineColorBlendAttachmentState structure with blending disabled
VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState_NoBlending(void);

VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState(
    bool blendEnable,
    VkBlendFactor srcColorBlendFactor,
    VkBlendFactor dstColorBlendFactor,
    VkBlendOp colorBlendOp,
    VkBlendFactor srcAlphaBlendFactor,
    VkBlendFactor dstAlphaBlendFactor,
    VkBlendOp alphaBlendOp,
    VkColorComponentFlags colorWriteMask);

VkPipelineColorBlendStateCreateInfo ivkGetPipelineColorBlendStateCreateInfo(
    uint32_t numAttachments,
    const VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates);

/// @brief Creates a VkPipelineViewportStateCreateInfo structure. If the viewport state is dyanamoc,
/// the parameters `viewport` and `scissor` may be NULL
VkPipelineViewportStateCreateInfo ivkGetPipelineViewportStateCreateInfo(const VkViewport* viewport,
                                                                        const VkRect2D* scissor);

/// @brief Creates a VkImageSubresourceRange structure for the first layer and mip level
VkImageSubresourceRange ivkGetImageSubresourceRange(VkImageAspectFlags aspectMask);

VkWriteDescriptorSet ivkGetWriteDescriptorSet_ImageInfo(VkDescriptorSet dstSet,
                                                        uint32_t dstBinding,
                                                        VkDescriptorType descriptorType,
                                                        uint32_t numDescriptors,
                                                        const VkDescriptorImageInfo* pImageInfo);

VkWriteDescriptorSet ivkGetWriteDescriptorSet_BufferInfo(VkDescriptorSet dstSet,
                                                         uint32_t dstBinding,
                                                         VkDescriptorType descriptorType,
                                                         uint32_t numDescriptors,
                                                         const VkDescriptorBufferInfo* pBufferInfo);

VkPipelineLayoutCreateInfo ivkGetPipelineLayoutCreateInfo(uint32_t numLayouts,
                                                          const VkDescriptorSetLayout* layouts,
                                                          const VkPushConstantRange* range);

VkPushConstantRange ivkGetPushConstantRange(VkShaderStageFlags stageFlags,
                                            size_t offset,
                                            size_t size);

VkViewport ivkGetViewport(float x, float y, float width, float height);

VkRect2D ivkGetRect2D(int32_t x, int32_t y, uint32_t width, uint32_t height);

VkPipelineShaderStageCreateInfo ivkGetPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                    VkShaderModule shaderModule,
                                                                    const char* entryPoint);

VkImageCopy ivkGetImageCopy2D(VkOffset2D srcDstOffset,
                              VkImageSubresourceLayers srcDstImageSubresource,
                              const VkExtent2D imageRegion);

VkBufferImageCopy ivkGetBufferImageCopy2D(uint32_t bufferOffset,
                                          uint32_t bufferRowLength,
                                          const VkRect2D imageRegion,
                                          VkImageSubresourceLayers imageSubresource);
VkBufferImageCopy ivkGetBufferImageCopy3D(uint32_t bufferOffset,
                                          uint32_t bufferRowLength,
                                          const VkOffset3D offset,
                                          const VkExtent3D extent,
                                          VkImageSubresourceLayers imageSubresource);

void ivkImageMemoryBarrier(const struct VulkanFunctionTable* vt,
                           VkCommandBuffer buffer,
                           VkImage image,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           VkImageLayout oldImageLayout,
                           VkImageLayout newImageLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkImageSubresourceRange subresourceRange);

void ivkBufferMemoryBarrier(const struct VulkanFunctionTable* vt,
                            VkCommandBuffer cmdBuffer,
                            VkBuffer buffer,
                            VkAccessFlags srcAccessMask,
                            VkAccessFlags dstAccessMask,
                            VkDeviceSize offset,
                            VkDeviceSize size,
                            VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask);

void ivkBufferBarrier(const struct VulkanFunctionTable* vt,
                      VkCommandBuffer cmdBuffer,
                      VkBuffer buffer,
                      VkBufferUsageFlags usageFlags,
                      VkPipelineStageFlags srcStageMask,
                      VkPipelineStageFlags dstStageMask);

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
                     VkFilter filter);

VkResult ivkQueuePresent(const struct VulkanFunctionTable* vt,
                         VkQueue graphicsQueue,
                         VkSemaphore waitSemaphore,
                         VkSwapchainKHR swapchain,
                         uint32_t currentSwapchainImageIndex);

/// @brief Adds a name for the Vulkan object with handle equals to `handle` and type equals to
/// `type`. This function is a no-op if `VK_EXT_DEBUG_UTILS_SUPPORTED` is not defined
VkResult ivkSetDebugObjectName(const struct VulkanFunctionTable* vt,
                               VkDevice device,
                               VkObjectType type,
                               uint64_t handle,
                               const char* name);

/// @brief Opens a command buffer debug region. This function is a no-op if
/// `VK_EXT_DEBUG_UTILS_SUPPORTED` is not defined
void ivkCmdBeginDebugUtilsLabel(const struct VulkanFunctionTable* vt,
                                VkCommandBuffer buffer,
                                const char* name,
                                const float colorRGBA[4]);

/// @brief Inserts a debug label into a command buffer. This function is a no-op if
/// `VK_EXT_DEBUG_UTILS_SUPPORTED` is not defined
void ivkCmdInsertDebugUtilsLabel(const struct VulkanFunctionTable* vt,
                                 VkCommandBuffer buffer,
                                 const char* name,
                                 const float colorRGBA[4]);

/// @brief Closes a command buffer debug region. This function is a no-op if
/// `VK_EXT_DEBUG_UTILS_SUPPORTED` is not defined
void ivkCmdEndDebugUtilsLabel(const struct VulkanFunctionTable* vt, VkCommandBuffer buffer);

VkVertexInputBindingDescription ivkGetVertexInputBindingDescription(uint32_t binding,
                                                                    uint32_t stride,
                                                                    VkVertexInputRate inputRate);
VkVertexInputAttributeDescription ivkGetVertexInputAttributeDescription(uint32_t location,
                                                                        uint32_t binding,
                                                                        VkFormat format,
                                                                        uint32_t offset);

VkResult ivkVmaCreateAllocator(const struct VulkanFunctionTable* vt,
                               VkPhysicalDevice physDev,
                               VkDevice device,
                               VkInstance instance,
                               uint32_t apiVersion,
                               bool enableBufferDeviceAddress,
                               VkDeviceSize preferredLargeHeapBlockSize,
                               VmaAllocator* outVma);

void ivkUpdateGlslangResource(glslang_resource_t* res, const VkPhysicalDeviceProperties* props);

#ifdef __cplusplus
}
#endif
