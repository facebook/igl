/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// This is a very low-level C wrapper of the Vulkan API to make it slightly less painful to use.

#pragma once

#include <glslang/Include/glslang_c_interface.h>

#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif // !defined(VK_NO_PROTOTYPES)

#include <volk.h>

#include <igl/Macros.h>
#include <igl/vulkan/VulkanVma.h>

#define IGL_ARRAY_NUM_ELEMENTS(x) (sizeof(x) / sizeof((x)[0]))

#ifdef __cplusplus
extern "C" {
#endif

const char* ivkGetVulkanResultString(VkResult result);

bool ivkIsValidationAvailable(void);

VkResult ivkCreateInstance(uint32_t apiVersion,
                           uint32_t enableValidation,
                           uint32_t enableGPUAssistedValidation,
                           uint32_t enableSynchronizationValidation,
                           size_t numExtensions,
                           const char** extensions,
                           VkInstance* outInstance);

VkResult ivkCreateDebugUtilsMessenger(VkInstance instance,
                                      PFN_vkDebugUtilsMessengerCallbackEXT callback,
                                      void* logUserData,
                                      VkDebugUtilsMessengerEXT* outMessenger);

// This function uses VK_EXT_debug_report extension, which is deprecated by VK_EXT_debug_utils.
// However, it is available on some Android devices where VK_EXT_debug_utils is not available.
VkResult ivkCreateDebugReportMessenger(VkInstance instance,
                                       PFN_vkDebugReportCallbackEXT callback,
                                       void* logUserData,
                                       VkDebugReportCallbackEXT* outMessenger);

VkResult ivkCreateSemaphore(VkDevice device, VkSemaphore* outSemaphore);

VkResult ivkCreateFence(VkDevice device, VkFlags flags, VkFence* outFence);

VkResult ivkCreateSurface(VkInstance instance,
                          void* window,
                          void* display,
                          VkSurfaceKHR* outSurface);

VkResult ivkCreateDevice(VkPhysicalDevice physicalDevice,
                         size_t numQueueCreateInfos,
                         const VkDeviceQueueCreateInfo* queueCreateInfos,
                         size_t numDeviceExtensions,
                         const char** deviceExtensions,
                         VkBool32 enableMultiview,
                         VkBool32 enableShaderFloat16,
                         VkBool32 enableBufferDeviceAddress,
                         VkDevice* outDevice);

VkResult ivkCreateHeadlessSurface(VkInstance instance, VkSurfaceKHR* surface);

VkResult ivkCreateSwapchain(VkDevice device,
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

VkResult ivkCreateSampler(VkDevice device, VkSampler* outSampler);

VkSamplerCreateInfo ivkGetSamplerCreateInfo(VkFilter minFilter,
                                            VkFilter magFilter,
                                            VkSamplerMipmapMode mipmapMode,
                                            VkSamplerAddressMode addressModeU,
                                            VkSamplerAddressMode addressModeV,
                                            VkSamplerAddressMode addressModeW,
                                            float minLod,
                                            float maxLod);

VkResult ivkCreateImageView(VkDevice device,
                            VkImage image,
                            VkImageViewType type,
                            VkFormat imageFormat,
                            VkImageSubresourceRange range,
                            VkImageView* outImageView);

VkResult ivkCreateFramebuffer(VkDevice device,
                              uint32_t width,
                              uint32_t height,
                              VkRenderPass renderPass,
                              size_t numAttachments,
                              const VkImageView* attachments,
                              VkFramebuffer* outFramebuffer);

VkResult ivkCreateCommandPool(VkDevice device,
                              VkCommandPoolCreateFlags flags,
                              uint32_t queueFamilyIndex,
                              VkCommandPool* outCommandPool);

VkResult ivkAllocateCommandBuffer(VkDevice device,
                                  VkCommandPool commandPool,
                                  VkCommandBuffer* outCommandBuffer);

VkResult ivkAllocateMemory(VkPhysicalDevice physDev,
                           VkDevice device,
                           const VkMemoryRequirements* memRequirements,
                           VkMemoryPropertyFlags props,
                           bool enableBufferDeviceAddress,
                           VkDeviceMemory* outMemory);

bool ivkIsHostVisibleSingleHeapMemory(VkPhysicalDevice physDev);

uint32_t ivkFindMemoryType(VkPhysicalDevice physDev,
                           uint32_t memoryTypeBits,
                           VkMemoryPropertyFlags flags);

VkResult ivkCreateRenderPass(VkDevice device,
                             uint32_t numAttachments,
                             const VkAttachmentDescription* attachments,
                             const VkSubpassDescription* subpass,
                             const VkSubpassDependency* dependency,
                             const VkRenderPassMultiviewCreateInfo* renderPassMultiview,
                             VkRenderPass* outRenderPass);

VkResult ivkCreateShaderModule(VkDevice device,
                               glslang_program_t* program,
                               VkShaderModule* outShaderModule);

VkResult ivkCreateShaderModuleFromSPIRV(VkDevice device,
                                        const void* dataSPIRV,
                                        size_t size,
                                        VkShaderModule* outShaderModule);

VkResult ivkCreateGraphicsPipeline(VkDevice device,
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

VkResult ivkCreateComputePipeline(VkDevice device,
                                  VkPipelineCache pipelineCache,
                                  const VkPipelineShaderStageCreateInfo* shaderStage,
                                  VkPipelineLayout pipelineLayout,
                                  VkPipeline* outPipeline);

VkResult ivkCreateDescriptorSetLayout(VkDevice device,
                                      uint32_t numBindings,
                                      const VkDescriptorSetLayoutBinding* bindings,
                                      const VkDescriptorBindingFlags* bindingFlags,
                                      VkDescriptorSetLayout* outLayout);

VkDescriptorSetLayoutBinding ivkGetDescriptorSetLayoutBinding(uint32_t binding,
                                                              VkDescriptorType descriptorType,
                                                              uint32_t descriptorCount);

VkAttachmentDescription ivkGetAttachmentDescription(VkFormat format,
                                                    VkAttachmentLoadOp loadOp,
                                                    VkAttachmentStoreOp storeOp,
                                                    VkImageLayout initialLayout,
                                                    VkImageLayout finalLayout,
                                                    VkSampleCountFlagBits samples);

VkAttachmentReference ivkGetAttachmentReference(uint32_t attachment, VkImageLayout layout);

VkSubpassDescription ivkGetSubpassDescription(uint32_t numColorAttachments,
                                              const VkAttachmentReference* refsColor,
                                              const VkAttachmentReference* refsColorResolve,
                                              const VkAttachmentReference* refDepth);

VkSubpassDependency ivkGetSubpassDependency(void);

VkRenderPassMultiviewCreateInfo ivkGetRenderPassMultiviewCreateInfo(
    const uint32_t* viewMask,
    const uint32_t* correlationMask);

VkResult ivkAllocateDescriptorSet(VkDevice device,
                                  VkDescriptorPool pool,
                                  VkDescriptorSetLayout layout,
                                  VkDescriptorSet* outDescriptorSet);

VkResult ivkCreateDescriptorPool(VkDevice device,
                                 uint32_t maxDescriptorSets,
                                 uint32_t numPoolSizes,
                                 const VkDescriptorPoolSize* poolSizes,
                                 VkDescriptorPool* outDescriptorPool);

VkResult ivkBeginCommandBuffer(VkCommandBuffer buffer);

VkResult ivkEndCommandBuffer(VkCommandBuffer buffer);

VkSubmitInfo ivkGetSubmitInfo(const VkCommandBuffer* buffer,
                              uint32_t numWaitSemaphores,
                              const VkSemaphore* waitSemaphores,
                              const VkPipelineStageFlags* waitStageMasks,
                              const VkSemaphore* releaseSemaphore);

VkAttachmentDescription2 ivkGetAttachmentDescriptionColor(VkFormat format,
                                                          VkAttachmentLoadOp loadOp,
                                                          VkAttachmentStoreOp storeOp,
                                                          VkImageLayout initialLayout,
                                                          VkImageLayout finalLayout);

VkAttachmentReference2 ivkGetAttachmentReferenceColor(uint32_t idx);

VkClearValue ivkGetClearDepthStencilValue(float depth, uint32_t stencil);
VkClearValue ivkGetClearColorValue(float r, float g, float b, float a);

VkBufferCreateInfo ivkGetBufferCreateInfo(uint64_t size, VkBufferUsageFlags usage);

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

VkPipelineRasterizationStateCreateInfo ivkGetPipelineRasterizationStateCreateInfo(
    VkPolygonMode polygonMode,
    VkCullModeFlags cullMode);

VkPipelineMultisampleStateCreateInfo ivkGetPipelineMultisampleStateCreateInfo_Empty(void);

VkPipelineDepthStencilStateCreateInfo ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests(
    void);

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

VkPipelineViewportStateCreateInfo ivkGetPipelineViewportStateCreateInfo(const VkViewport* viewport,
                                                                        const VkRect2D* scissor);

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
                                          const VkRect2D imageRegion,
                                          VkImageSubresourceLayers imageSubresource);
VkBufferImageCopy ivkGetBufferImageCopy3D(uint32_t bufferOffset,
                                          const VkOffset3D offset,
                                          const VkExtent3D extent,
                                          VkImageSubresourceLayers imageSubresource);

glslang_input_t ivkGetGLSLangInput(VkShaderStageFlagBits stage,
                                   const glslang_resource_t* resource,
                                   const char* shaderCode);

void ivkImageMemoryBarrier(VkCommandBuffer buffer,
                           VkImage image,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           VkImageLayout oldImageLayout,
                           VkImageLayout newImageLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkImageSubresourceRange subresourceRange);

void ivkBufferMemoryBarrier(VkCommandBuffer cmdBuffer,
                            VkBuffer buffer,
                            VkAccessFlags srcAccessMask,
                            VkAccessFlags dstAccessMask,
                            VkDeviceSize offset,
                            VkDeviceSize size,
                            VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask);

void ivkCmdBlitImage(VkCommandBuffer buffer,
                     VkImage srcImage,
                     VkImage dstImage,
                     VkImageLayout srcImageLayout,
                     VkImageLayout dstImageLayout,
                     const VkOffset3D* srcOffsets,
                     const VkOffset3D* dstOffsets,
                     VkImageSubresourceLayers srcSubresourceRange,
                     VkImageSubresourceLayers dstSubresourceRange,
                     VkFilter filter);

VkResult ivkQueuePresent(VkQueue graphicsQueue,
                         VkSemaphore waitSemaphore,
                         VkSwapchainKHR swapchain,
                         uint32_t currentSwapchainImageIndex);

VkResult ivkSetDebugObjectName(VkDevice device,
                               VkObjectType type,
                               uint64_t handle,
                               const char* name);

void ivkCmdBeginDebugUtilsLabel(VkCommandBuffer buffer, const char* name, const float colorRGBA[4]);

void ivkCmdInsertDebugUtilsLabel(VkCommandBuffer buffer,
                                 const char* name,
                                 const float colorRGBA[4]);

void ivkCmdEndDebugUtilsLabel(VkCommandBuffer buffer);

VkVertexInputBindingDescription ivkGetVertexInputBindingDescription(uint32_t binding,
                                                                    uint32_t stride,
                                                                    VkVertexInputRate inputRate);
VkVertexInputAttributeDescription ivkGetVertexInputAttributeDescription(uint32_t location,
                                                                        uint32_t binding,
                                                                        VkFormat format,
                                                                        uint32_t offset);

VkResult ivkVmaCreateAllocator(VkPhysicalDevice physDev,
                               VkDevice device,
                               VkInstance instance,
                               uint32_t apiVersion,
                               bool enableBufferDeviceAddress,
                               VmaAllocator* outVma);

void ivkGlslangResource(glslang_resource_t* glslangResource,
                        const VkPhysicalDeviceProperties* deviceProperties);

#ifdef __cplusplus
}
#endif
