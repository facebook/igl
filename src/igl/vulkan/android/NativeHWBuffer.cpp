/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "NativeHWBuffer.h"

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <android/hardware_buffer.h>
#include <vulkan/vulkan_android.h>

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan::android {

namespace {
uint32_t ivkGetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties& memProps,
                               const uint32_t typeBits,
                               const VkMemoryPropertyFlags requiredProperties) {
  // Search memory types to find the index with the requested properties.
  for (uint32_t type = 0; type < memProps.memoryTypeCount; type++) {
    if ((typeBits & (1 << type)) != 0) {
      // Test if this memory type has the required properties.
      const VkFlags propertyFlags = memProps.memoryTypes[type].propertyFlags;
      if ((propertyFlags & requiredProperties) == requiredProperties) {
        return type;
      }
    }
  }
  return 0;
}
} // namespace

NativeHWTextureBuffer::NativeHWTextureBuffer(igl::vulkan::Device& device, TextureFormat format) :
  Super(device, format) {}

NativeHWTextureBuffer::~NativeHWTextureBuffer() {}

Result NativeHWTextureBuffer::create(const TextureDesc& desc) {
  return createHWBuffer(desc, false, false);
}

Result NativeHWTextureBuffer::createTextureInternal(AHardwareBuffer* hwBuffer) {
  if (hwBuffer == nullptr) {
    return Result(Result::Code::RuntimeError, "null buffer passed to create texture");
  }
  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(hwBuffer, &hwbDesc);
  TextureDesc desc;
  desc.width = hwbDesc.width;
  desc.height = hwbDesc.height;
  desc.usage = ::igl::android::getIglBufferUsage(hwbDesc.usage);

  auto& ctx = device_.getVulkanContext();
  auto device = device_.getVulkanContext().getVkDevice();
  auto physicalDevice = device_.getVulkanContext().getVkPhysicalDevice();
  VkImageCreateFlags create_flags = 0;
  if (hwbDesc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) {
    create_flags |= VK_IMAGE_CREATE_PROTECTED_BIT;
  }
  VkImageUsageFlags usage_flags = 0;
  if (hwbDesc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
    usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (hwbDesc.usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) {
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // There is no matching usage flag in AHardwareBuffer to match VK_IMAGE_USAGE_STORAGE_BIT in
    // Vulkan. So we assume if the the color output flag is set for the buffer, we
    // enable the storage usage.
    usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }

  VkAndroidHardwareBufferFormatPropertiesANDROID ahb_format_props = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
      .pNext = nullptr,
  };
  VkAndroidHardwareBufferPropertiesANDROID ahb_props = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
      .pNext = &ahb_format_props,
  };

  VK_ASSERT(ctx.vf_.vkGetAndroidHardwareBufferPropertiesANDROID(device, hwBuffer, &ahb_props));

  VkExternalFormatANDROID external_format = {
      VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
  };

  if (ahb_format_props.format == VK_FORMAT_UNDEFINED) {
    external_format.externalFormat = ahb_format_props.externalFormat;
  }

  VkExternalMemoryImageCreateInfo external_memory_image_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .pNext = &external_format,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
  };

  desc.format = igl::vulkan::vkFormatToTextureFormat(ahb_format_props.format);

  VkImage vk_image;

  VkImageCreateInfo vk_image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                     .pNext = &external_memory_image_info,
                                     .flags = create_flags,
                                     .imageType = VK_IMAGE_TYPE_2D,
                                     .format = ahb_format_props.format,
                                     .extent =
                                         VkExtent3D{(uint32_t)desc.width, (uint32_t)desc.height, 1},
                                     .mipLevels = 1,
                                     .arrayLayers = 1,
                                     .samples = VK_SAMPLE_COUNT_1_BIT,
                                     .tiling = VK_IMAGE_TILING_OPTIMAL,
                                     .usage = usage_flags,
                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                     .queueFamilyIndexCount = 0,
                                     .pQueueFamilyIndices = nullptr,
                                     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
  // Create Vk Image.
  VK_ASSERT(ctx.vf_.vkCreateImage(device, &vk_image_info, nullptr, &vk_image));

  if (vk_image == VK_NULL_HANDLE) {
    IGL_LOG_ERROR("failed to create image view format is %d and external format is %d",
                  vk_image_info.format,
                  external_format.externalFormat);
    return Result(Result::Code::RuntimeError, "Failed to create vulkan image");
  }

  // To import memory created outside of the current Vulkan instance from an
  // Android hardware buffer, add a VkImportAndroidHardwareBufferInfoANDROID
  // structure to the pNext chain of the VkMemoryAllocateInfo structure.
  VkImportAndroidHardwareBufferInfoANDROID ahb_import_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
      .pNext = nullptr,
      .buffer = hwBuffer};

  // If the VkMemoryAllocateInfo pNext chain includes a
  // VkMemoryDedicatedAllocateInfo structure, then that structure includes a
  // handle of the sole buffer or image resource that the memory can be bound
  // to.
  VkMemoryDedicatedAllocateInfo dedicated_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .pNext = &ahb_import_info,
      .image = vk_image,
      .buffer = VK_NULL_HANDLE};

  // Find the memory type that supports the required properties.
  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  ctx.vf_.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &vulkanMemoryProperties);

  uint32_t memory_type_bits = ahb_props.memoryTypeBits;

  uint32_t type_index = ivkGetMemoryTypeIndex(
      vulkanMemoryProperties, memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // An instance of the VkMemoryAllocateInfo structure defines a memory import
  // operation.
  VkMemoryAllocateInfo mem_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &dedicated_alloc_info,
      // If the parameters define an import operation and the external handle type
      // is VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
      // allocationSize must be the size returned by
      // vkGetAndroidHardwareBufferPropertiesANDROID for the Android hardware
      // buffer.
      .allocationSize = ahb_props.allocationSize,
      .memoryTypeIndex = type_index};

  // A Vulkan device operates on data in device memory via memory objects that
  // are represented in the API by a VkDeviceMemory handle.
  // Allocate memory.
  VkDeviceMemory vk_device_memory;
  VK_ASSERT(ctx.vf_.vkAllocateMemory(device, &mem_alloc_info, nullptr, &vk_device_memory));

  // Attach memory to the image object.
  VK_ASSERT(ctx.vf_.vkBindImageMemory(device, vk_image, vk_device_memory, 0));

  auto vulkanImage = VulkanImage(ctx,
                                 device,
                                 vk_image,
                                 "Image View: videoTexture",
                                 usage_flags,
                                 false,
                                 vk_image_info.extent,
                                 vk_image_info.imageType,
                                 vk_image_info.format,
                                 vk_image_info.mipLevels,
                                 vk_image_info.arrayLayers,
                                 VK_SAMPLE_COUNT_1_BIT,
                                 true);
  vulkanImage.vkMemory_[0] = vk_device_memory;
  vulkanImage.extendedFormat_ = external_format.externalFormat;

  VkImageViewCreateInfo viewInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vk_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = vk_image_info.format,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = vk_image_info.mipLevels,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
  };

  viewInfo.pNext = nullptr;
  VkSamplerYcbcrConversionInfo conversionInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
      .pNext = nullptr,
      .conversion = VK_NULL_HANDLE,
  };

  if (ahb_format_props.format == VK_FORMAT_UNDEFINED && external_format.externalFormat) {
    viewInfo.pNext = &conversionInfo;
    VkSamplerYcbcrConversionCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
        .pNext = &external_format,
        .format = ahb_format_props.format,
        .ycbcrModel = ahb_format_props.suggestedYcbcrModel,
        .ycbcrRange = ahb_format_props.suggestedYcbcrRange,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .xChromaOffset = ahb_format_props.suggestedXChromaOffset,
        .yChromaOffset = ahb_format_props.suggestedYChromaOffset,
        .chromaFilter = VK_FILTER_LINEAR,
        .forceExplicitReconstruction = VK_FALSE};

    ctx.vf_.vkCreateSamplerYcbcrConversion(
        device, &createInfo, nullptr, &conversionInfo.conversion);
    IGL_LOG_INFO("created sampler ycbcr conversion at %x with %d %d %d and %d",
                 conversionInfo.conversion,
                 ahb_format_props.suggestedYcbcrModel,
                 ahb_format_props.suggestedYcbcrRange,
                 ahb_format_props.suggestedXChromaOffset,
                 ahb_format_props.suggestedYChromaOffset);
  } else if (igl::vulkan::getNumImagePlanes(ahb_format_props.format) > 1) {
    auto createInfo = ctx.getOrCreateYcbcrConversionInfo(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
    conversionInfo.conversion = createInfo.conversion;
  }

  VulkanImageView vulkanImageView(ctx, viewInfo, "Image View: videoTexture");

  auto vkTexture = device_.getVulkanContext().createTexture(
      std::move(vulkanImage), std::move(vulkanImageView), "SurfaceTexture");

  if (!vkTexture) {
    return Result(Result::Code::RuntimeError, "Failed to create vulkan texture");
  }

  desc_ = std::move(desc);
  texture_ = std::move(vkTexture);

  return Result{Result::Code::Ok};
}

} // namespace igl::vulkan::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
