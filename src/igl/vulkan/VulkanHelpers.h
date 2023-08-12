/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// This is a very low-level C wrapper of the Vulkan API to make it slightly less painful to use.

#pragma once

#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif // !defined(VK_NO_PROTOTYPES)

#include <stdbool.h>
#include <volk.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* ivkGetVulkanResultString(VkResult result);

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

VkResult ivkAllocateMemory(VkPhysicalDevice physDev,
                           VkDevice device,
                           const VkMemoryRequirements* memRequirements,
                           VkMemoryPropertyFlags props,
                           VkDeviceMemory* outMemory);

bool ivkIsHostVisibleSingleHeapMemory(VkPhysicalDevice physDev);

uint32_t ivkFindMemoryType(VkPhysicalDevice physDev, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags);

VkResult ivkCreateDescriptorSetLayout(VkDevice device,
                                      uint32_t numBindings,
                                      const VkDescriptorSetLayoutBinding* bindings,
                                      const VkDescriptorBindingFlags* bindingFlags,
                                      VkDescriptorSetLayout* outLayout);

VkDescriptorSetLayoutBinding ivkGetDescriptorSetLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount);

VkAttachmentDescription ivkGetAttachmentDescription(VkFormat format,
                                                    VkAttachmentLoadOp loadOp,
                                                    VkAttachmentStoreOp storeOp,
                                                    VkImageLayout initialLayout,
                                                    VkImageLayout finalLayout,
                                                    VkSampleCountFlagBits samples);

VkAttachmentReference ivkGetAttachmentReference(uint32_t attachment, VkImageLayout layout);

VkResult ivkAllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet* outDescriptorSet);

VkResult ivkCreateDescriptorPool(VkDevice device,
                                 uint32_t maxDescriptorSets,
                                 uint32_t numPoolSizes,
                                 const VkDescriptorPoolSize* poolSizes,
                                 VkDescriptorPool* outDescriptorPool);

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

VkPipelineVertexInputStateCreateInfo ivkGetPipelineVertexInputStateCreateInfo(uint32_t vbCount,
                                                                              const VkVertexInputBindingDescription* bindings,
                                                                              uint32_t vaCount,
                                                                              const VkVertexInputAttributeDescription* attributes);

VkPipelineInputAssemblyStateCreateInfo ivkGetPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology,
                                                                                  VkBool32 enablePrimitiveRestart);

VkPipelineDynamicStateCreateInfo ivkGetPipelineDynamicStateCreateInfo(uint32_t numDynamicStates, const VkDynamicState* dynamicStates);

VkPipelineRasterizationStateCreateInfo ivkGetPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlags cullMode);

VkPipelineMultisampleStateCreateInfo ivkGetPipelineMultisampleStateCreateInfo_Empty(void);

VkPipelineDepthStencilStateCreateInfo ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests(void);

VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState_NoBlending(void);

VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState(bool blendEnable,
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

VkPipelineViewportStateCreateInfo ivkGetPipelineViewportStateCreateInfo(const VkViewport* viewport, const VkRect2D* scissor);

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

VkRect2D ivkGetRect2D(int32_t x, int32_t y, uint32_t width, uint32_t height);

VkPipelineShaderStageCreateInfo ivkGetPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                    VkShaderModule shaderModule,
                                                                    const char* entryPoint);

VkImageCopy ivkGetImageCopy2D(VkOffset2D srcDstOffset, VkImageSubresourceLayers srcDstImageSubresource, const VkExtent2D imageRegion);

VkBufferImageCopy ivkGetBufferImageCopy2D(uint32_t bufferOffset, const VkRect2D imageRegion, VkImageSubresourceLayers imageSubresource);
VkBufferImageCopy ivkGetBufferImageCopy3D(uint32_t bufferOffset,
                                          const VkOffset3D offset,
                                          const VkExtent3D extent,
                                          VkImageSubresourceLayers imageSubresource);

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

VkResult ivkSetDebugObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name);

#ifdef __cplusplus
}
#endif
