/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#define VMA_VULKAN_VERSION 1003000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif // !defined(VK_NO_PROTOTYPES)

#include <volk.h>
#include <vk_mem_alloc.h>
#include <lvk/LVK.h>

#define VK_ASSERT(func)                                            \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      LLOGW("Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
                    __FILE__,                                      \
                    __LINE__,                                      \
                    #func,                                         \
                    lvk::getVulkanResultString(vk_assert_result)); \
      assert(false);                                               \
    }                                                              \
  }

#define VK_ASSERT_RETURN(func)                                     \
  {                                                                \
    const VkResult vk_assert_result = func;                        \
    if (vk_assert_result != VK_SUCCESS) {                          \
      LLOGW("Vulkan API call failed: %s:%i\n  %s\n  %s\n", \
                    __FILE__,                                      \
                    __LINE__,                                      \
                    #func,                                         \
                    lvk::getVulkanResultString(vk_assert_result)); \
      assert(false);                                               \
      return getResultFromVkResult(vk_assert_result);              \
    }                                                              \
  }

typedef struct glslang_resource_s glslang_resource_t;

namespace lvk {

VkSemaphore createSemaphore(VkDevice device, const char* debugName);
VkFence createFence(VkDevice device, const char* debugName);
VmaAllocator createVmaAllocator(VkPhysicalDevice physDev, VkDevice device, VkInstance instance, uint32_t apiVersion);
uint32_t findQueueFamilyIndex(VkPhysicalDevice physDev, VkQueueFlags flags);
VkResult setDebugObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name);
VkResult allocateMemory(VkPhysicalDevice physDev,
                        VkDevice device,
                        const VkMemoryRequirements* memRequirements,
                        VkMemoryPropertyFlags props,
                        VkDeviceMemory* outMemory);

glslang_resource_t getGlslangResource(const VkPhysicalDeviceLimits& limits);
Result compileShader(VkDevice device,
                     VkShaderStageFlagBits stage,
                     const char* code,
                     VkShaderModule* outShaderModule,
                     const glslang_resource_t* glslLangResource = nullptr);

VkSamplerCreateInfo samplerStateDescToVkSamplerCreateInfo(const lvk::SamplerStateDesc& desc, const VkPhysicalDeviceLimits& limits);
VkDescriptorSetLayoutBinding getDSLBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount);
VkPipelineShaderStageCreateInfo getPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                 VkShaderModule shaderModule,
                                                                 const char* entryPoint);
void imageMemoryBarrier(VkCommandBuffer buffer,
                        VkImage image,
                        VkAccessFlags srcAccessMask,
                        VkAccessFlags dstAccessMask,
                        VkImageLayout oldImageLayout,
                        VkImageLayout newImageLayout,
                        VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask,
                        VkImageSubresourceRange subresourceRange);

Result getResultFromVkResult(VkResult result);
const char* getVulkanResultString(VkResult result);
lvk::Format vkFormatToFormat(VkFormat format);
VkFormat formatToVkFormat(lvk::Format format);

} // namespace lvk
