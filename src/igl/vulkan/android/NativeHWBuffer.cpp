/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only
// @fb-only

#include "NativeHWBuffer.h"

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <android/hardware_buffer.h>
#include <vulkan/vulkan_android.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

#if IGL_LOGGING_ENABLED
#include <mutex>
#endif

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

#if IGL_LOGGING_ENABLED
const char* vkFormatName(const VkFormat format) noexcept {
#define MP_VK_NAME(value) \
  case value:             \
    return #value
  switch (format) {
    MP_VK_NAME(VK_FORMAT_UNDEFINED);
    MP_VK_NAME(VK_FORMAT_R8G8B8A8_UNORM);
    MP_VK_NAME(VK_FORMAT_B8G8R8A8_UNORM);
    MP_VK_NAME(VK_FORMAT_R8G8B8_UNORM);
    MP_VK_NAME(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
    MP_VK_NAME(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
  default:
    return "VK_FORMAT_UNKNOWN";
  }
#undef MP_VK_NAME
}

const char* vkComponentSwizzleName(const VkComponentSwizzle swizzle) noexcept {
#define MP_VK_NAME(value) \
  case value:             \
    return #value
  switch (swizzle) {
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_IDENTITY);
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_ZERO);
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_ONE);
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_R);
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_G);
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_B);
    MP_VK_NAME(VK_COMPONENT_SWIZZLE_A);
  default:
    return "VK_COMPONENT_SWIZZLE_UNKNOWN";
  }
#undef MP_VK_NAME
}

const char* vkYcbcrModelName(const VkSamplerYcbcrModelConversion model) noexcept {
#define MP_VK_NAME(value) \
  case value:             \
    return #value
  switch (model) {
    MP_VK_NAME(VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY);
    MP_VK_NAME(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY);
    MP_VK_NAME(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709);
    MP_VK_NAME(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601);
    MP_VK_NAME(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020);
  default:
    return "VK_SAMPLER_YCBCR_MODEL_CONVERSION_UNKNOWN";
  }
#undef MP_VK_NAME
}

const char* vkYcbcrRangeName(const VkSamplerYcbcrRange range) noexcept {
#define MP_VK_NAME(value) \
  case value:             \
    return #value
  switch (range) {
    MP_VK_NAME(VK_SAMPLER_YCBCR_RANGE_ITU_FULL);
    MP_VK_NAME(VK_SAMPLER_YCBCR_RANGE_ITU_NARROW);
  default:
    return "VK_SAMPLER_YCBCR_RANGE_UNKNOWN";
  }
#undef MP_VK_NAME
}

const char* vkDriverIdName(const VkDriverId driverId) noexcept {
#define MP_VK_NAME(value) \
  case value:             \
    return #value
  switch (driverId) {
    MP_VK_NAME(VK_DRIVER_ID_QUALCOMM_PROPRIETARY);
    MP_VK_NAME(VK_DRIVER_ID_ARM_PROPRIETARY);
    MP_VK_NAME(VK_DRIVER_ID_GOOGLE_SWIFTSHADER);
    MP_VK_NAME(VK_DRIVER_ID_MESA_RADV);
    MP_VK_NAME(VK_DRIVER_ID_MESA_LLVMPIPE);
  default:
    return "VK_DRIVER_ID_UNKNOWN";
  }
#undef MP_VK_NAME
}

void logFirstAhbVulkanDiagnostics(const VkAndroidHardwareBufferFormatPropertiesANDROID& formatProps,
                                  const VkPhysicalDeviceDriverPropertiesKHR& driverProps) {
  const VkComponentMapping& components = formatProps.samplerYcbcrConversionComponents;
  if (formatProps.format == VK_FORMAT_UNDEFINED) {
    IGL_LOG_DEBUG(
        "[vulkan][ahb][first_frame] Vulkan AHB import\n"
        "  format=%s (%d) externalFormat=0x%llx\n"
        "  components r=%s g=%s b=%s a=%s\n"
        "  ycbcrModel=%s ycbcrRange=%s\n"
        "  driverID=%s (%d) conformance=%u.%u.%u.%u\n",
        vkFormatName(formatProps.format),
        static_cast<int>(formatProps.format),
        static_cast<unsigned long long>(formatProps.externalFormat),
        vkComponentSwizzleName(components.r),
        vkComponentSwizzleName(components.g),
        vkComponentSwizzleName(components.b),
        vkComponentSwizzleName(components.a),
        vkYcbcrModelName(formatProps.suggestedYcbcrModel),
        vkYcbcrRangeName(formatProps.suggestedYcbcrRange),
        vkDriverIdName(driverProps.driverID),
        static_cast<int>(driverProps.driverID),
        static_cast<unsigned>(driverProps.conformanceVersion.major),
        static_cast<unsigned>(driverProps.conformanceVersion.minor),
        static_cast<unsigned>(driverProps.conformanceVersion.subminor),
        static_cast<unsigned>(driverProps.conformanceVersion.patch));
    return;
  }

  IGL_LOG_DEBUG(
      "[vulkan][ahb][first_frame] Vulkan AHB import\n"
      "  format=%s (%d)\n"
      "  components r=%s g=%s b=%s a=%s\n"
      "  ycbcrModel=%s ycbcrRange=%s\n"
      "  driverID=%s (%d) conformance=%u.%u.%u.%u\n",
      vkFormatName(formatProps.format),
      static_cast<int>(formatProps.format),
      vkComponentSwizzleName(components.r),
      vkComponentSwizzleName(components.g),
      vkComponentSwizzleName(components.b),
      vkComponentSwizzleName(components.a),
      vkYcbcrModelName(formatProps.suggestedYcbcrModel),
      vkYcbcrRangeName(formatProps.suggestedYcbcrRange),
      vkDriverIdName(driverProps.driverID),
      static_cast<int>(driverProps.driverID),
      static_cast<unsigned>(driverProps.conformanceVersion.major),
      static_cast<unsigned>(driverProps.conformanceVersion.minor),
      static_cast<unsigned>(driverProps.conformanceVersion.subminor),
      static_cast<unsigned>(driverProps.conformanceVersion.patch));
}
#endif // IGL_LOGGING_ENABLED
} // namespace

NativeHWTextureBuffer::NativeHWTextureBuffer(igl::vulkan::Device& device, TextureFormat format) :
  Super(device, format) {}

NativeHWTextureBuffer::~NativeHWTextureBuffer() = default;

VkSamplerYcbcrConversion NativeHWTextureBuffer::getVkSamplerYcbcrConversion() const noexcept {
  // Null texture_ and null ycbcrConversion_ are both valid "not available" states.
  if (texture_ == nullptr) {
    return VK_NULL_HANDLE;
  }
  return texture_->image.ycbcrConversion_;
}

Result NativeHWTextureBuffer::create(const TextureDesc& desc) {
  return createHWBuffer(desc, false, false);
}

Result NativeHWTextureBuffer::createTextureInternal(AHardwareBuffer* hwBuffer) {
  if (hwBuffer == nullptr) {
    return Result(Result::Code::RuntimeError, "null buffer passed to create texture");
  }
  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(hwBuffer, &hwbDesc);

  auto& ctx = device_.getVulkanContext();
  auto device = device_.getVulkanContext().getVkDevice();
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
  }
  if (hwbDesc.usage & AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER) {
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

#if IGL_LOGGING_ENABLED
  static std::once_flag firstAhbVulkanDiagnosticsOnce;
  std::call_once(firstAhbVulkanDiagnosticsOnce, [&ctx, &ahb_format_props]() {
    logFirstAhbVulkanDiagnostics(ahb_format_props, ctx.getVkPhysicalDeviceDriverProperties());
  });
#endif

  VkExternalFormatANDROID external_format = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
  };

  if (ahb_format_props.format == VK_FORMAT_UNDEFINED) {
    external_format.externalFormat = ahb_format_props.externalFormat;
  }

  VkExternalMemoryImageCreateInfo external_memory_image_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .pNext = &external_format,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
  };

  auto desc = TextureDesc::newNativeHWBufferImage(
      igl::vulkan::vkFormatToTextureFormat(ahb_format_props.format),
      igl::android::getIglBufferUsage(hwbDesc.usage),
      hwbDesc.width,
      hwbDesc.height);

  VkImage vk_image;

  VkImageCreateInfo vk_image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = &external_memory_image_info,
      .flags = create_flags,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = ahb_format_props.format,
      .extent = {.width = (uint32_t)desc.width, .height = (uint32_t)desc.height, .depth = 1},
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

  VK_ASSERT(ivkSetDebugObjectName(&ctx.vf_,
                                  device,
                                  VK_OBJECT_TYPE_IMAGE,
                                  (uint64_t)vk_image,
                                  "Image: AHB NativeHWTextureBuffer"));

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
  uint32_t memory_type_bits = ahb_props.memoryTypeBits;

  uint32_t type_index = ivkGetMemoryTypeIndex(
      ctx.memoryProperties, memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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
                                 vk_image,
                                 "Image: videoTexture",
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
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
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
    // Use the driver-reported raw-plane swizzle, with a legacy Qualcomm-Adreno correction for
    // Camera2 YUV_420_888 streams whose reported IDENTITY mapping still swaps Cb/Cr.
    // VK_KHR_driver_properties gives a narrow gate; zeroed pre-1.2 properties skip the workaround.
    VkComponentMapping components = ahb_format_props.samplerYcbcrConversionComponents;
    const VkPhysicalDeviceDriverPropertiesKHR& driverProps =
        ctx.getVkPhysicalDeviceDriverProperties();
    const bool isQualcommDriver = driverProps.driverID == VK_DRIVER_ID_QUALCOMM_PROPRIETARY;
    const bool isLegacyConformance =
        driverProps.conformanceVersion.major < 1 ||
        (driverProps.conformanceVersion.major == 1 && driverProps.conformanceVersion.minor < 3);
    if (isQualcommDriver && isLegacyConformance) {
      components.r = VK_COMPONENT_SWIZZLE_B;
      components.b = VK_COMPONENT_SWIZZLE_R;
    }
    vulkanImage.samplerYcbcrConversionCreateInfo_ = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
        .pNext = &external_format,
        .format = ahb_format_props.format,
        .ycbcrModel = ahb_format_props.suggestedYcbcrModel,
        .ycbcrRange = ahb_format_props.suggestedYcbcrRange,
        .components = components,
        .xChromaOffset = ahb_format_props.suggestedXChromaOffset,
        .yChromaOffset = ahb_format_props.suggestedYChromaOffset,
        .chromaFilter = VK_FILTER_LINEAR,
        .forceExplicitReconstruction = VK_FALSE};

    // Reuse the context-owned conversion so AImageReader pool slots share one handle.
    // This avoids rebuilding immutable-sampler pipelines when the slot rotates.
    conversionInfo.conversion =
        ctx.getOrCreateExternalYcbcrConversion(vulkanImage.samplerYcbcrConversionCreateInfo_);
    if (conversionInfo.conversion == VK_NULL_HANDLE) {
      return Result(Result::Code::RuntimeError,
                    "Failed to create external-format YCbCr conversion for AHB import");
    }
    // Expose the non-owning handle to consumers that need a matching VkSampler.
    vulkanImage.ycbcrConversion_ = conversionInfo.conversion;
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

  desc_ = desc; // Field within the Texture class
  textureDesc_ = desc; // Field within the NativeHWTextureBuffer class
  texture_ = std::move(vkTexture);

  return Result{Result::Code::Ok};
}

} // namespace igl::vulkan::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
