/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // VK_USE_PLATFORM_WIN32_KHR

#if defined(VOLK_HEADER_ONLY)
#define VOLK_IMPLEMENTATION
#endif // VOLK_HEADER_ONLY

#include "VulkanHelpers.h"

#include <assert.h>

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

VkResult ivkAllocateMemory(VkPhysicalDevice physDev,
                           VkDevice device,
                           const VkMemoryRequirements* memRequirements,
                           VkMemoryPropertyFlags props,
                           VkDeviceMemory* outMemory) {
  assert(memRequirements);

  const VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
  };

  const VkMemoryAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = memRequirements->size,
      .memoryTypeIndex = ivkFindMemoryType(physDev, memRequirements->memoryTypeBits, props),
  };

  return vkAllocateMemory(device, &ai, NULL, outMemory);
}

bool ivkIsHostVisibleSingleHeapMemory(VkPhysicalDevice physDev) {
  VkPhysicalDeviceMemoryProperties memProperties;

  vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

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

uint32_t ivkFindMemoryType(VkPhysicalDevice physDev, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    const bool hasProperties = (memProperties.memoryTypes[i].propertyFlags & flags) == flags;
    if ((memoryTypeBits & (1 << i)) && hasProperties) {
      return i;
    }
  }

  assert(false);

  return 0;
}

static void ivkAddNext(void* node, const void* next) {
  if (!node || !next) {
    return;
  }

  VkBaseInStructure* cur = (VkBaseInStructure*)node;

  while (cur->pNext) {
    cur = (VkBaseInStructure*)cur->pNext;
  }

  cur->pNext = next;
}

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
                            VkSwapchainKHR* outSwapchain) {
  assert(caps);
  const bool isCompositeAlphaOpaqueSupported = (caps->supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0;
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
      .compositeAlpha = isCompositeAlphaOpaqueSupported ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };
  return vkCreateSwapchainKHR(device, &ci, NULL, outSwapchain);
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

VkResult ivkCreateImageView(VkDevice device,
                            VkImage image,
                            VkImageViewType type,
                            VkFormat imageFormat,
                            VkImageSubresourceRange range,
                            VkImageView* outImageView) {
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

  return vkCreateImageView(device, &ci, NULL, outImageView);
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

VkDescriptorSetLayoutBinding ivkGetDescriptorSetLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount) {
  const VkDescriptorSetLayoutBinding bind = {
      .binding = binding,
      .descriptorType = descriptorType,
      .descriptorCount = descriptorCount,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
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

VkResult ivkCreateDescriptorSetLayout(VkDevice device,
                                      uint32_t numBindings,
                                      const VkDescriptorSetLayoutBinding* bindings,
                                      const VkDescriptorBindingFlags* bindingFlags,
                                      VkDescriptorSetLayout* outLayout) {
  const VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlagsCI = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
      .bindingCount = numBindings,
      .pBindingFlags = bindingFlags,
  };

  const VkDescriptorSetLayoutCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = &setLayoutBindingFlagsCI,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
      .bindingCount = numBindings,
      .pBindings = bindings,
  };
  return vkCreateDescriptorSetLayout(device, &ci, NULL, outLayout);
}

VkResult ivkAllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet* outDescriptorSet) {
  const VkDescriptorSetAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout,
  };

  return vkAllocateDescriptorSets(device, &ai, outDescriptorSet);
}

VkResult ivkCreateDescriptorPool(VkDevice device,
                                 uint32_t maxDescriptorSets,
                                 uint32_t numPoolSizes,
                                 const VkDescriptorPoolSize* poolSizes,
                                 VkDescriptorPool* outDescriptorPool) {
  const VkDescriptorPoolCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = maxDescriptorSets,
      .poolSizeCount = numPoolSizes,
      .pPoolSizes = poolSizes,
  };
  return vkCreateDescriptorPool(device, &ci, NULL, outDescriptorPool);
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

VkPipelineVertexInputStateCreateInfo ivkGetPipelineVertexInputStateCreateInfo(uint32_t vbCount,
                                                                              const VkVertexInputBindingDescription* bindings,
                                                                              uint32_t vaCount,
                                                                              const VkVertexInputAttributeDescription* attributes) {
  const VkPipelineVertexInputStateCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                   .vertexBindingDescriptionCount = vbCount,
                                                   .pVertexBindingDescriptions = bindings,
                                                   .vertexAttributeDescriptionCount = vaCount,
                                                   .pVertexAttributeDescriptions = attributes};
  return ci;
}

VkPipelineInputAssemblyStateCreateInfo ivkGetPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology,
                                                                                  VkBool32 enablePrimitiveRestart) {
  const VkPipelineInputAssemblyStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .flags = 0,
      .topology = topology,
      .primitiveRestartEnable = enablePrimitiveRestart,
  };
  return ci;
}

VkPipelineDynamicStateCreateInfo ivkGetPipelineDynamicStateCreateInfo(uint32_t numDynamicStates, const VkDynamicState* dynamicStates) {
  const VkPipelineDynamicStateCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = numDynamicStates,
      .pDynamicStates = dynamicStates,
  };
  return ci;
}

VkPipelineViewportStateCreateInfo ivkGetPipelineViewportStateCreateInfo(const VkViewport* viewport, const VkRect2D* scissor) {
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

VkPipelineRasterizationStateCreateInfo ivkGetPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode,
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

VkPipelineDepthStencilStateCreateInfo ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests(void) {
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
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  return state;
}

VkPipelineColorBlendAttachmentState ivkGetPipelineColorBlendAttachmentState(bool blendEnable,
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

VkWriteDescriptorSet ivkGetWriteDescriptorSet_BufferInfo(VkDescriptorSet dstSet,
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
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = range,
  };
  return ci;
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

VkRect2D ivkGetRect2D(int32_t x, int32_t y, uint32_t width, uint32_t height) {
  const VkRect2D rect = {
      .offset = {.x = x, .y = y},
      .extent = {.width = width, .height = height},
  };
  return rect;
}

void ivkImageMemoryBarrier(VkCommandBuffer buffer,
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
  vkCmdPipelineBarrier(buffer, srcStageMask, dstStageMask, 0, 0, NULL, 0, NULL, 1, &barrier);
}

void ivkBufferMemoryBarrier(VkCommandBuffer cmdBuffer,
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
  vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, 0, 0, NULL, 1, &barrier, 0, NULL);
}

void ivkCmdBlitImage(VkCommandBuffer buffer,
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
  vkCmdBlitImage(buffer, srcImage, srcImageLayout, dstImage, dstImageLayout, 1, &blit, filter);
}

VkResult ivkSetDebugObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
  if (!name || !*name) {
    return VK_SUCCESS;
  }
  const VkDebugUtilsObjectNameInfoEXT ni = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = handle,
      .pObjectName = name,
  };
  return vkSetDebugUtilsObjectNameEXT(device, &ni);
}

void ivkCmdBeginDebugUtilsLabel(VkCommandBuffer buffer, const char* name, const float colorRGBA[4]) {
  if (!name) {
    return;
  }
  const VkDebugUtilsLabelEXT label = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = NULL,
      .pLabelName = name,
      .color = {colorRGBA[0], colorRGBA[1], colorRGBA[2], colorRGBA[3]},
  };
  vkCmdBeginDebugUtilsLabelEXT(buffer, &label);
}

void ivkCmdInsertDebugUtilsLabel(VkCommandBuffer buffer, const char* name, const float colorRGBA[4]) {
  if (!name) {
    return;
  }
  const VkDebugUtilsLabelEXT label = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = NULL,
      .pLabelName = name,
      .color = {colorRGBA[0], colorRGBA[1], colorRGBA[2], colorRGBA[3]},
  };
  vkCmdInsertDebugUtilsLabelEXT(buffer, &label);
}

void ivkCmdEndDebugUtilsLabel(VkCommandBuffer buffer) {
  vkCmdEndDebugUtilsLabelEXT(buffer);
}

VkBufferImageCopy ivkGetBufferImageCopy2D(uint32_t bufferOffset, const VkRect2D imageRegion, VkImageSubresourceLayers imageSubresource) {
  const VkBufferImageCopy copy = {
      .bufferOffset = bufferOffset,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = imageSubresource,
      .imageOffset = {.x = imageRegion.offset.x, .y = imageRegion.offset.y, .z = 0},
      .imageExtent = {.width = imageRegion.extent.width, .height = imageRegion.extent.height, .depth = 1u},
  };
  return copy;
}

VkBufferImageCopy ivkGetBufferImageCopy3D(uint32_t bufferOffset,
                                          const VkOffset3D offset,
                                          const VkExtent3D extent,
                                          VkImageSubresourceLayers imageSubresource) {
  const VkBufferImageCopy copy = {
      .bufferOffset = bufferOffset,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = imageSubresource,
      .imageOffset = offset,
      .imageExtent = extent,
  };
  return copy;
}

VkImageCopy ivkGetImageCopy2D(VkOffset2D srcDstOffset, VkImageSubresourceLayers srcDstImageSubresource, const VkExtent2D imageRegion) {
  const VkImageCopy copy = {
      .srcSubresource = srcDstImageSubresource,
      .srcOffset = {.x = srcDstOffset.x, .y = srcDstOffset.y, .z = 0},
      .dstSubresource = srcDstImageSubresource,
      .dstOffset = {.x = srcDstOffset.x, .y = srcDstOffset.y, .z = 0},
      .extent = {.width = imageRegion.width, .height = imageRegion.height, .depth = 1u},
  };
  return copy;
}
