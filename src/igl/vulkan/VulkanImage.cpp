/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImage.h"

#include <array>
#include <cinttypes>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImageView.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#include <unistd.h>
#endif

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
#include <android/hardware_buffer.h>
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

// any image layout transition causes a full barrier
#define IGL_DEBUG_ENFORCE_FULL_IMAGE_BARRIER 0

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
  IGL_LOG_ERROR("Memory type %d with properties %d not found.", typeBits, requiredProperties);
  return 0;
}

// VkImage export and import is only implemented on Windows, Linux and Android platforms.
#if IGL_PLATFORM_WIN
constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
#elif IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

} // namespace

namespace igl::vulkan {

VulkanImage::VulkanImage(const VulkanContext& ctx,
                         VkDevice device,
                         VkImage image,
                         const char* debugName,
                         VkImageUsageFlags usageFlags,
                         bool isExternallyManaged,
                         VkExtent3D extent,
                         VkImageType type,
                         VkFormat imageFormat,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         VkSampleCountFlagBits samples,
                         bool isImported) :
  ctx_(&ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  vkImage_(image),
  usageFlags_(usageFlags),
  isExternallyManaged_(isExternallyManaged),
  extent_(extent),
  type_(type),
  imageFormat_(imageFormat),
  mipLevels_(mipLevels),
  arrayLayers_(arrayLayers),
  samples_(samples),
  isDepthFormat_(isDepthFormat(imageFormat)),
  isStencilFormat_(isStencilFormat(imageFormat)),
  isDepthOrStencilFormat_(isDepthFormat_ || isStencilFormat_),
  isImported_(isImported) {
  setName(debugName);
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));
}
VulkanImage::VulkanImage(const VulkanContext& ctx,
                         VkDevice device,
                         VkImage image,
                         const VulkanImageCreateInfo& createInfo,
                         const char* debugName) :
  VulkanImage(ctx,
              device,
              image,
              debugName,
              createInfo.usageFlags,
              createInfo.isExternallyManaged,
              createInfo.extent,
              createInfo.type,
              createInfo.imageFormat,
              createInfo.mipLevels,
              createInfo.arrayLayers,
              createInfo.samples,
              createInfo.isImported) {}

VulkanImage::VulkanImage(const VulkanContext& ctx,
                         VkDevice device,
                         VkExtent3D extent,
                         VkImageType type,
                         VkFormat format,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         VkImageTiling tiling,
                         VkImageUsageFlags usageFlags,
                         VkMemoryPropertyFlags memFlags,
                         VkImageCreateFlags createFlags,
                         VkSampleCountFlagBits samples,
                         const char* debugName) :
  ctx_(&ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  usageFlags_(usageFlags),
  extent_(extent),
  type_(type),
  imageFormat_(format),
  mipLevels_(mipLevels),
  arrayLayers_(arrayLayers),
  samples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)),
  isDepthOrStencilFormat_(isDepthFormat_ || isStencilFormat_),
  isCubemap_((createFlags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0),
  tiling_(tiling) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT_MSG(mipLevels_ > 0, "The image must contain at least one mip level");
  IGL_ASSERT_MSG(arrayLayers_ > 0, "The image must contain at least one layer");
  IGL_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  IGL_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");

  setName(debugName);

  const bool isDisjoint = (createFlags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0;

  const VkImageCreateInfo ci = ivkGetImageCreateInfo(type,
                                                     imageFormat_,
                                                     tiling,
                                                     usageFlags,
                                                     extent_,
                                                     mipLevels_,
                                                     arrayLayers_,
                                                     createFlags,
                                                     samples);

  if (IGL_VULKAN_USE_VMA && !isDisjoint) {
    VmaAllocationCreateInfo ciAlloc = {};

    ciAlloc.usage = memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? VMA_MEMORY_USAGE_CPU_TO_GPU
                                                                   : VMA_MEMORY_USAGE_AUTO;

    const VkResult result = vmaCreateImage(
        (VmaAllocator)ctx_->getVmaAllocator(), &ci, &ciAlloc, &vkImage_, &vmaAllocation_, nullptr);

    if (!IGL_VERIFY(result == VK_SUCCESS)) {
      IGL_LOG_ERROR("failed: error result: %d, memflags: %d,  imageformat: %d\n",
                    result,
                    memFlags,
                    imageFormat_);
    }

    VkMemoryRequirements memRequirements;
    ctx_->vf_.vkGetImageMemoryRequirements(device, vkImage_, &memRequirements);

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, &mappedPtr_);
      if (memRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        isCoherentMemory_ = true;
      }
    }

    if (vmaAllocation_) {
      VmaAllocationInfo allocationInfo;
      vmaGetAllocationInfo((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, &allocationInfo);
      allocatedSize = allocationInfo.size;
    }
  } else {
    // create a disjoint image - TODO: merge it with the VMA code path above
    VK_ASSERT(ctx_->vf_.vkCreateImage(device_, &ci, nullptr, &vkImage_));

    // Ignore clang-diagnostic-missing-field-initializers
    // @lint-ignore CLANGTIDY
    VkMemoryRequirements2 memRequirements[3] = {
        {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2},
        {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2},
        {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2},
    };

    // back the image with some memory
    {
      const uint32_t numPlanes = igl::vulkan::getNumImagePlanes(format);
      IGL_ASSERT(numPlanes > 0 && numPlanes <= kMaxImagePlanes);
      // @fb-only
      const VkImagePlaneMemoryRequirementsInfo planes[kMaxImagePlanes] = {
          ivkGetImagePlaneMemoryRequirementsInfo(VK_IMAGE_ASPECT_PLANE_0_BIT),
          ivkGetImagePlaneMemoryRequirementsInfo(VK_IMAGE_ASPECT_PLANE_1_BIT),
          ivkGetImagePlaneMemoryRequirementsInfo(VK_IMAGE_ASPECT_PLANE_2_BIT),
      };
      // @fb-only
      const VkImageMemoryRequirementsInfo2 imgRequirements[kMaxImagePlanes] = {
          ivkGetImageMemoryRequirementsInfo2(numPlanes > 0 ? &planes[0] : nullptr, vkImage_),
          ivkGetImageMemoryRequirementsInfo2(numPlanes > 1 ? &planes[1] : nullptr, vkImage_),
          ivkGetImageMemoryRequirementsInfo2(numPlanes > 2 ? &planes[2] : nullptr, vkImage_),
      };
      for (uint32_t p = 0; p != numPlanes; p++) {
        ctx_->vf_.vkGetImageMemoryRequirements2(device, &imgRequirements[p], &memRequirements[p]);
        VK_ASSERT(ivkAllocateMemory2(&ctx_->vf_,
                                     physicalDevice_,
                                     device_,
                                     &memRequirements[p],
                                     memFlags,
                                     false,
                                     &vkMemory_[p]));
      }
      // @fb-only
      const VkBindImagePlaneMemoryInfo bindImagePlaneMemoryInfo[kMaxImagePlanes] = {
          {VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, nullptr, VK_IMAGE_ASPECT_PLANE_0_BIT},
          {VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, nullptr, VK_IMAGE_ASPECT_PLANE_1_BIT},
          {VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, nullptr, VK_IMAGE_ASPECT_PLANE_2_BIT},
      };
      // @fb-only
      const VkBindImageMemoryInfo bindInfo[kMaxImagePlanes] = {
          ivkGetBindImageMemoryInfo(
              isDisjoint ? &bindImagePlaneMemoryInfo[0] : nullptr, vkImage_, vkMemory_[0]),
          ivkGetBindImageMemoryInfo(&bindImagePlaneMemoryInfo[1], vkImage_, vkMemory_[1]),
          ivkGetBindImageMemoryInfo(&bindImagePlaneMemoryInfo[2], vkImage_, vkMemory_[2]),
      };
      VK_ASSERT(ctx_->vf_.vkBindImageMemory2(device_, numPlanes, bindInfo));

      allocatedSize = memRequirements[0].memoryRequirements.size +
                      memRequirements[1].memoryRequirements.size +
                      memRequirements[2].memoryRequirements.size;
    }

    // handle memory-mapped images
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      // map only the first image plane
      VK_ASSERT(ctx_->vf_.vkMapMemory(device_, vkMemory_[0], 0, VK_WHOLE_SIZE, 0, &mappedPtr_));
      const uint32_t memoryTypeBits = memRequirements[0].memoryRequirements.memoryTypeBits;
      if (memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        isCoherentMemory_ = true;
      }
    }
  }

  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  // Get physical device's properties for the image's format
  ctx_->vf_.vkGetPhysicalDeviceFormatProperties(physicalDevice_, imageFormat_, &formatProperties_);
}

VulkanImage::VulkanImage(const VulkanContext& ctx,
                         int32_t undupedFileDescriptor,
                         uint64_t memoryAllocationSize,
                         VkDevice device,
                         VkExtent3D extent,
                         VkImageType type,
                         VkFormat format,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         VkImageTiling tiling,
                         VkImageUsageFlags usageFlags,
                         VkImageCreateFlags createFlags,
                         VkSampleCountFlagBits samples,
                         const char* debugName) :
  ctx_(&ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  usageFlags_(usageFlags),
  extent_(extent),
  type_(type),
  imageFormat_(format),
  mipLevels_(mipLevels),
  arrayLayers_(arrayLayers),
  samples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)),
  isDepthOrStencilFormat_(isDepthFormat_ || isStencilFormat_),
  isImported_(true),
  tiling_(tiling) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT_MSG(mipLevels_ > 0, "The image must contain at least one mip level");
  IGL_ASSERT_MSG(arrayLayers_ > 0, "The image must contain at least one layer");
  IGL_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  IGL_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");

#ifdef VK_USE_PLATFORM_WIN32_KHR
  IGL_ASSERT_MSG(false, "You can only import a VulkanImage on non-windows environments");
#endif

  setName(debugName);

  VkImageCreateInfo ci = ivkGetImageCreateInfo(type,
                                               imageFormat_,
                                               tiling,
                                               usageFlags,
                                               extent_,
                                               mipLevels_,
                                               arrayLayers_,
                                               createFlags,
                                               samples);

  VkExternalMemoryImageCreateInfoKHR extImgMem = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      nullptr,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR};

  ci.pNext = &extImgMem;

  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  ctx_->vf_.vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &vulkanMemoryProperties);

  // create image.. importing external memory cannot use VMA
  VK_ASSERT(ctx_->vf_.vkCreateImage(device_, &ci, nullptr, &vkImage_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  int importedFd = -1;
#ifndef VK_USE_PLATFORM_WIN32_KHR
  importedFd = dup(undupedFileDescriptor);
#endif
  IGL_ASSERT(importedFd >= 0);

  // NOTE: Importing memory from a file descriptor transfers ownership of the fd from the
  // app to the Vk implementation. The app must not perform any operations on the fd after
  // a successful import.
  // Vulkan implementation is responsible for closing the fds
  //
  // Apps can import the same underlying memory into:
  //  - multiple instances of vk
  //  - same instance from which it was exported
  //  - multiple times into a given vk instance.
  // in all cases, each import operation must create a distinct vkdevicememory object

  const VkImageMemoryRequirementsInfo2 memoryRequirementInfo = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, vkImage_};

  VkMemoryRequirements2 memoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  ctx_->vf_.vkGetImageMemoryRequirements2(device_, &memoryRequirementInfo, &memoryRequirements);

  // TODO_VULKAN: Verify the following from the spec:
  // the memory from which fd was exported must have been created on the same physical device
  // as device.
  VkImportMemoryFdInfoKHR fdInfo = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                    nullptr,
                                    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
                                    importedFd};

  const VkMemoryAllocateInfo memoryAllocateInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &fdInfo,
      memoryAllocationSize,
      ivkGetMemoryTypeIndex(vulkanMemoryProperties,
                            memoryRequirements.memoryRequirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

  IGL_LOG_INFO("Imported texture has requirements %d, ends up index %d",
               memoryRequirements.memoryRequirements.memoryTypeBits,
               memoryAllocateInfo.memoryTypeIndex);

  // @fb-only
  // @fb-only
// @fb-only
  // @fb-only
      // @fb-only
  // @fb-only
// @fb-only
  VK_ASSERT(ctx_->vf_.vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vkMemory_[0]));
  VK_ASSERT(ctx_->vf_.vkBindImageMemory(device_, vkImage_, vkMemory_[0], 0));
// @fb-only
}

#if IGL_PLATFORM_WIN
VulkanImage::VulkanImage(const VulkanContext& ctx,
                         void* windowsHandle,
                         VkDevice device,
                         VkExtent3D extent,
                         VkImageType type,
                         VkFormat format,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         VkImageTiling tiling,
                         VkImageUsageFlags usageFlags,
                         VkImageCreateFlags createFlags,
                         VkSampleCountFlagBits samples,
                         const char* debugName) :
  ctx_(&ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  usageFlags_(usageFlags),
  extent_(extent),
  type_(type),
  imageFormat_(format),
  mipLevels_(mipLevels),
  arrayLayers_(arrayLayers),
  samples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)),
  isDepthOrStencilFormat_(isDepthFormat_ || isStencilFormat_),
  isImported_(true),
  tiling_(tiling) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT_MSG(mipLevels_ > 0, "The image must contain at least one mip level");
  IGL_ASSERT_MSG(arrayLayers_ > 0, "The image must contain at least one layer");
  IGL_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  IGL_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");

  VkImageCreateInfo ci = ivkGetImageCreateInfo(type,
                                               imageFormat_,
                                               tiling,
                                               usageFlags,
                                               extent_,
                                               mipLevels_,
                                               arrayLayers_,
                                               createFlags,
                                               samples);

  const VkExternalMemoryImageCreateInfoKHR extImgMem = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      nullptr,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT};

  ci.pNext = &extImgMem;

  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  ctx_->vf_.vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &vulkanMemoryProperties);

  // create image.. importing external memory cannot use VMA
  VK_ASSERT(ctx_->vf_.vkCreateImage(device_, &ci, nullptr, &vkImage_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  const VkImageMemoryRequirementsInfo2 memoryRequirementInfo = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, vkImage_};

  VkMemoryRequirements2 memoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  ctx_->vf_.vkGetImageMemoryRequirements2(device_, &memoryRequirementInfo, &memoryRequirements);

  const VkImportMemoryWin32HandleInfoKHR handleInfo = {
      VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
      nullptr,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
      windowsHandle};

  const VkMemoryAllocateInfo memoryAllocateInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &handleInfo,
      memoryRequirements.memoryRequirements.size,
      ivkGetMemoryTypeIndex(vulkanMemoryProperties,
                            memoryRequirements.memoryRequirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

  IGL_LOG_INFO("Imported texture has memoryAllocationSize %" PRIu64
               ", requirements 0x%08X, ends up index 0x%08X",
               memoryRequirements.memoryRequirements.size,
               memoryRequirements.memoryRequirements.memoryTypeBits,
               memoryAllocateInfo.memoryTypeIndex);

  VK_ASSERT(ctx_->vf_.vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vkMemory_[0]));
  VK_ASSERT(ctx_->vf_.vkBindImageMemory(device_, vkImage_, vkMemory_[0], 0));
}
#endif // IGL_PLATFORM_WIN

#if IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
VulkanImage VulkanImage::createWithExportMemory(const VulkanContext& ctx,
                                                VkDevice device,
                                                VkExtent3D extent,
                                                VkImageType type,
                                                VkFormat format,
                                                uint32_t mipLevels,
                                                uint32_t arrayLayers,
                                                VkImageTiling tiling,
                                                VkImageUsageFlags usageFlags,
                                                VkImageCreateFlags createFlags,
                                                VkSampleCountFlagBits samples,
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                                                AHardwareBuffer* hwBuffer,
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                                                const char* debugName) {
  const VkPhysicalDeviceExternalImageFormatInfo externaInfo = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
      nullptr,
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
      hwBuffer != nullptr ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID :
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                          kHandleType,
  };
  const VkPhysicalDeviceImageFormatInfo2 formatInfo2 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      &externaInfo,
      format,
      VK_IMAGE_TYPE_2D,
      tiling,
      usageFlags,
      createFlags,
  };

  VkExternalImageFormatProperties externalImageFormatProperties = {
      VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
  VkImageFormatProperties2 imageFormatProperties2 = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
                                                     &externalImageFormatProperties};
  const auto result = ctx.vf_.vkGetPhysicalDeviceImageFormatProperties2(
      ctx.getVkPhysicalDevice(), &formatInfo2, &imageFormatProperties2);
  if (result != VK_SUCCESS) {
    IGL_LOG_ERROR(
        "External memory is not supported. format: %d image_tiling: %d usage: %d flags: %d",
        format,
        tiling,
        usageFlags,
        createFlags);
    return VulkanImage();
  }
  const auto& externalFormatProperties = externalImageFormatProperties.externalMemoryProperties;
  if (!(externalFormatProperties.externalMemoryFeatures &
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
    IGL_LOG_ERROR(
        "External memory cannot be exported. format: %d image_tiling: %d usage: %d flags: %d",
        format,
        tiling,
        usageFlags,
        createFlags);
    return VulkanImage();
  }
  const auto compatibleHandleTypes = externalFormatProperties.compatibleHandleTypes;

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  if (hwBuffer != nullptr) {
    IGL_ASSERT(compatibleHandleTypes &
               VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID);
  } else {
#endif
    IGL_ASSERT(compatibleHandleTypes & kHandleType);
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  }
#endif

  return {ctx,
          device,
          extent,
          type,
          format,
          mipLevels,
          arrayLayers,
          tiling,
          usageFlags,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          createFlags,
          samples,
          compatibleHandleTypes,
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
          hwBuffer,
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
          debugName};
}

VulkanImage::VulkanImage(const VulkanContext& ctx,
                         VkDevice device,
                         VkExtent3D extent,
                         VkImageType type,
                         VkFormat format,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         VkImageTiling tiling,
                         VkImageUsageFlags usageFlags,
                         VkMemoryPropertyFlags memFlags,
                         VkImageCreateFlags createFlags,
                         VkSampleCountFlagBits samples,
                         const VkExternalMemoryHandleTypeFlags compatibleHandleTypes,
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                         AHardwareBuffer* hwBuffer,
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                         const char* debugName) :
  ctx_(&ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  usageFlags_(usageFlags),
  extent_(extent),
  type_(type),
  imageFormat_(format),
  mipLevels_(mipLevels),
  arrayLayers_(arrayLayers),
  samples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)),
  isDepthOrStencilFormat_(isDepthFormat_ || isStencilFormat_),
  isExported_(true),
  tiling_(tiling) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT_MSG(mipLevels_ > 0, "The image must contain at least one mip level");
  IGL_ASSERT_MSG(arrayLayers_ > 0, "The image must contain at least one layer");
  IGL_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  IGL_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  VkExternalFormatANDROID externalFormat = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
      .externalFormat = 0,
  };

  if (hwBuffer != nullptr) {
    // Allocate vkMemory and all the associated structs
    // Example taken from:
    //
    // https://github.com/refi64/chromium-tar/blob/82ebd6a0473341fa75dd3bbb2f584da99f5ac92c/gpu/vulkan/vulkan_image_android.cc

    // Get format
    VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties = {
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
    };

    // Buffer Properties
    VkAndroidHardwareBufferPropertiesANDROID bufferProperties = {
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, &formatProperties};
    bufferProperties.pNext = &formatProperties;

    VK_ASSERT(ctx_->vf_.vkGetAndroidHardwareBufferPropertiesANDROID(
        device_, hwBuffer, &bufferProperties));

    // If image has an external format, format must be VK_FORMAT_UNDEFINED.
    if (formatProperties.format == VK_FORMAT_UNDEFINED) {
      externalFormat.externalFormat = formatProperties.externalFormat;
    }
  }
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

  const VkExternalMemoryImageCreateInfoKHR externalImageCreateInfo = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
      hwBuffer != nullptr ? &externalFormat :
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                          nullptr,
      compatibleHandleTypes,
  };

  VkImageCreateInfo ci = ivkGetImageCreateInfo(type,
                                               imageFormat_,
                                               tiling,
                                               usageFlags,
                                               extent_,
                                               mipLevels_,
                                               arrayLayers_,
                                               createFlags,
                                               samples);

  ci.pNext = &externalImageCreateInfo;

  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  ctx_->vf_.vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &vulkanMemoryProperties);

  // create VkImage importing external memory cannot use VMA
  VK_ASSERT(ctx_->vf_.vkCreateImage(device_, &ci, nullptr, &vkImage_));
  VK_ASSERT(ivkSetDebugObjectName(
      &ctx_->vf_, device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  // For Android we need a dedicated allocation for exporting the image, otherwise
  // the exported handle is not generated properly.
#if IGL_PLATFORM_ANDROID

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  VkImportAndroidHardwareBufferInfoANDROID bufferInfo = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
      .pNext = nullptr,
      .buffer = hwBuffer,
  };
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

  VkMemoryDedicatedAllocateInfoKHR dedicatedAllocateInfo = {
      VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
      hwBuffer != nullptr ? &bufferInfo :
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
                          nullptr,
      vkImage_,
      VK_NULL_HANDLE,
  };
#endif // IGL_PLATFORM_ANDROID

  const VkExportMemoryAllocateInfoKHR externalMemoryAllocateInfo = {
      VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
#if IGL_PLATFORM_ANDROID
      &dedicatedAllocateInfo,
#else
      nullptr,
#endif // IGL_PLATFORM_ANDROID
      compatibleHandleTypes,
  };

  std::array<VkBindImagePlaneMemoryInfo, kMaxImagePlanes> bindImagePlaneMemoryInfo{};
  std::array<VkBindImageMemoryInfo, kMaxImagePlanes> bindInfo{};
  const uint32_t numPlanes = igl::vulkan::getNumImagePlanes(format);
  IGL_ASSERT(numPlanes > 0 && numPlanes <= kMaxImagePlanes);
  for (uint32_t p = 0; p != numPlanes; p++) {
    auto imagePlaneMemoryRequirementsInfo = ivkGetImagePlaneMemoryRequirementsInfo(
        (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << p));

    const VkImageMemoryRequirementsInfo2 imageMemoryRequirementInfo = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        numPlanes > 1 ? &imagePlaneMemoryRequirementsInfo : nullptr,
        vkImage_};

    VkMemoryRequirements2 memoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr};
    ctx_->vf_.vkGetImageMemoryRequirements2(
        device_, &imageMemoryRequirementInfo, &memoryRequirements);

    const VkMemoryAllocateInfo memoryAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        &externalMemoryAllocateInfo,
        memoryRequirements.memoryRequirements.size,
        ivkGetMemoryTypeIndex(vulkanMemoryProperties,
                              memoryRequirements.memoryRequirements.memoryTypeBits,
                              memFlags)};

    IGL_LOG_INFO("Creating image to be exported with memoryAllocationSize %" PRIu64
                 ", requirements 0x%08X, ends up index 0x%08X",
                 memoryRequirements.memoryRequirements.size,
                 memoryRequirements.memoryRequirements.memoryTypeBits,
                 memoryAllocateInfo.memoryTypeIndex);

    VK_ASSERT(ctx_->vf_.vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vkMemory_[p]));

    bindImagePlaneMemoryInfo[p] =
        VkBindImagePlaneMemoryInfo{VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                   nullptr,
                                   (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << p)};
    bindInfo[p] = ivkGetBindImageMemoryInfo(
        numPlanes > 1 ? &bindImagePlaneMemoryInfo[p] : nullptr, vkImage_, vkMemory_[p]);
  }
  VK_ASSERT(ctx_->vf_.vkBindImageMemory2(device_, numPlanes, bindInfo.data()));

#if IGL_PLATFORM_WIN
  const VkMemoryGetWin32HandleInfoKHR getHandleInfo{
      VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR, nullptr, vkMemory_[0], kHandleType};
  VK_ASSERT(ctx_->vf_.vkGetMemoryWin32HandleKHR(device_, &getHandleInfo, &exportedMemoryHandle_));
#else
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  if (hwBuffer == nullptr) {
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
    const VkMemoryGetFdInfoKHR getFdInfo{.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
                                         .pNext = nullptr,
                                         .memory = vkMemory_[0],
                                         .handleType = kHandleType};
    VK_ASSERT(ctx_->vf_.vkGetMemoryFdKHR(device_, &getFdInfo, &exportedFd_));
#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  }
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
#endif
}
#endif // IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID

VulkanImage::~VulkanImage() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  destroy();
}

void VulkanImage::destroy() {
  if (!valid()) {
    return;
  }

  if (!isExternallyManaged_) {
    if (vkMemory_[1] == VK_NULL_HANDLE) {
      if (vmaAllocation_) {
        if (mappedPtr_) {
          vmaUnmapMemory((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_);
        }
        ctx_->deferredTask(std::packaged_task<void()>(
            [vma = ctx_->getVmaAllocator(), image = vkImage_, allocation = vmaAllocation_]() {
              vmaDestroyImage((VmaAllocator)vma, image, allocation);
            }));
      } else {
        if (mappedPtr_) {
          ctx_->vf_.vkUnmapMemory(device_, vkMemory_[0]);
        }
        ctx_->deferredTask(std::packaged_task<void()>(
            [vf = &ctx_->vf_, device = device_, image = vkImage_, memory = vkMemory_[0]]() {
              vf->vkDestroyImage(device, image, nullptr);
              if (memory != VK_NULL_HANDLE) {
                vf->vkFreeMemory(device, memory, nullptr);
              }
            }));
      }
    } else {
      // this never uses VMA
      if (mappedPtr_) {
        ctx_->vf_.vkUnmapMemory(device_, vkMemory_[0]);
      }
      ctx_->deferredTask(std::packaged_task<void()>([vf = &ctx_->vf_,
                                                     device = device_,
                                                     image = vkImage_,
                                                     memory0 = vkMemory_[0],
                                                     memory1 = vkMemory_[1],
                                                     memory2 = vkMemory_[2]]() {
        vf->vkDestroyImage(device, image, nullptr);
        vf->vkFreeMemory(device, memory0, nullptr);
        vf->vkFreeMemory(device, memory1, nullptr);
        vf->vkFreeMemory(device, memory2, nullptr);
      }));
    }
  }

  ctx_ = nullptr;
  vkImage_ = VK_NULL_HANDLE;
}

VulkanImageView VulkanImage::createImageView(VkImageViewType type,
                                             VkFormat format,
                                             VkImageAspectFlags aspectMask,
                                             uint32_t baseLevel,
                                             uint32_t numLevels,
                                             uint32_t baseLayer,
                                             uint32_t numLayers,
                                             const char* debugName) const {
  return {*ctx_,
          vkImage_,
          type,
          format,
          aspectMask,
          baseLevel,
          numLevels ? numLevels : mipLevels_,
          baseLayer,
          numLayers,
          debugName};
}

VulkanImageView VulkanImage::createImageView(VulkanImageViewCreateInfo createInfo,
                                             const char* debugName) const {
  return {*ctx_, device_, vkImage_, createInfo, debugName};
}

void VulkanImage::transitionLayout(VkCommandBuffer cmdBuf,
                                   VkImageLayout newImageLayout,
                                   VkPipelineStageFlags srcStageMask,
                                   VkPipelineStageFlags dstStageMask,
                                   const VkImageSubresourceRange& subresourceRange) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_TRANSITION);

  VkAccessFlags srcAccessMask = 0;
  VkAccessFlags dstAccessMask = 0;

  if (imageLayout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
    // we do not need to wait for any previous operations in this case
    srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }

  const VkPipelineStageFlags doNotRequireAccessMask =
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT |
      VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  VkPipelineStageFlags srcRemainingMask = srcStageMask & ~doNotRequireAccessMask;
  VkPipelineStageFlags dstRemainingMask = dstStageMask & ~doNotRequireAccessMask;

  if (srcStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcRemainingMask &= ~VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcRemainingMask &= ~VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
    srcRemainingMask &= ~VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
    srcRemainingMask &= ~VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) {
    srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcRemainingMask &= ~VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
    srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    srcRemainingMask &= ~VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  IGL_ASSERT_MSG(
      srcRemainingMask == 0,
      "Automatic access mask deduction is not implemented (yet) for this srcStageMask = %u",
      srcRemainingMask);

  if (dstStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) {
    dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) {
    dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;
    dstAccessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) {
    dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
    dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
    dstRemainingMask &= ~VK_PIPELINE_STAGE_TRANSFER_BIT;
  }

  IGL_ASSERT_MSG(
      dstRemainingMask == 0,
      "Automatic access mask deduction is not implemented (yet) for this dstStageMask = %u",
      dstRemainingMask);

#if IGL_DEBUG_ENFORCE_FULL_IMAGE_BARRIER
  // full image barrier
  srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

  srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
#endif // IGL_DEBUG_ENFORCE_FULL_IMAGE_BARRIER

  ivkImageMemoryBarrier(&ctx_->vf_,
                        cmdBuf,
                        vkImage_,
                        srcAccessMask,
                        dstAccessMask,
                        imageLayout_,
                        newImageLayout,
                        srcStageMask,
                        dstStageMask,
                        subresourceRange);

  imageLayout_ = newImageLayout;
}

void VulkanImage::clearColorImage(VkCommandBuffer commandBuffer,
                                  const igl::Color& rgba,
                                  const VkImageSubresourceRange* subresourceRange) const {
  IGL_ASSERT(usageFlags_ & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  IGL_ASSERT(samples_ == VK_SAMPLE_COUNT_1_BIT);
  IGL_ASSERT(!isDepthOrStencilFormat_);

  const VkImageLayout oldLayout = imageLayout_;

  VkClearColorValue value;
  value.float32[0] = rgba.r;
  value.float32[1] = rgba.g;
  value.float32[2] = rgba.b;
  value.float32[3] = rgba.a;

  const VkImageSubresourceRange defaultRange{
      getImageAspectFlags(),
      0,
      VK_REMAINING_MIP_LEVELS,
      0,
      VK_REMAINING_ARRAY_LAYERS,
  };

  transitionLayout(commandBuffer,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   subresourceRange ? *subresourceRange : defaultRange);

  ctx_->vf_.vkCmdClearColorImage(commandBuffer,
                                 getVkImage(),
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &value,
                                 1,
                                 subresourceRange ? subresourceRange : &defaultRange);

  const VkImageLayout newLayout =
      oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
          ? (usageFlags_ & VK_IMAGE_USAGE_SAMPLED_BIT ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
          : oldLayout;

  transitionLayout(commandBuffer,
                   newLayout,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   subresourceRange ? *subresourceRange : defaultRange);
}

VkImageAspectFlags VulkanImage::getImageAspectFlags() const {
  VkImageAspectFlags flags = 0;

  if (isDepthFormat_) {
    flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (isStencilFormat_) {
    flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  if (!isDepthOrStencilFormat_) {
    flags |= VK_IMAGE_ASPECT_COLOR_BIT;
  }

  return flags;
}

void VulkanImage::generateMipmap(VkCommandBuffer commandBuffer,
                                 const TextureRangeDesc& range) const {
  IGL_PROFILER_FUNCTION();

  // Check if device supports downscaling for color or depth/stencil buffer based on image format
  {
    const uint32_t formatFeatureMask =
        (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

    const bool hardwareDownscalingSupported =
        ((formatProperties_.optimalTilingFeatures & formatFeatureMask) == formatFeatureMask);

    if (!IGL_VERIFY(hardwareDownscalingSupported)) {
      IGL_ASSERT_MSG(false,
                     IGL_FORMAT("Doesn't support hardware downscaling of this image format: {}",
                                uint32_t(imageFormat_))
                         .c_str());
      return;
    }
  }

  // Choose linear filter for color formats if supported by the device, else use nearest filter
  // Choose nearest filter by default for depth/stencil formats
  const VkFilter blitFilter =
      [](bool isDepthOrStencilFormat, bool imageFilterLinear) {
        if (isDepthOrStencilFormat) {
          return VK_FILTER_NEAREST;
        }
        if (imageFilterLinear) {
          return VK_FILTER_LINEAR;
        }
        return VK_FILTER_NEAREST;
      }(isDepthOrStencilFormat_,
        (formatProperties_.optimalTilingFeatures &
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0u);

  const VkImageAspectFlags imageAspectFlags = getImageAspectFlags();

  ivkCmdBeginDebugUtilsLabel(
      &ctx_->vf_, commandBuffer, "Generate mipmaps", kColorGenerateMipmaps.toFloatPtr());

  IGL_SCOPE_EXIT {
    ivkCmdEndDebugUtilsLabel(&ctx_->vf_, commandBuffer);
  };

  const VkImageLayout originalImageLayout = imageLayout_;

  IGL_ASSERT(originalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

  IGL_ASSERT_MSG(!isCubemap_ || arrayLayers_ % 6u == 0,
                 "Cubemaps must have a multiple of 6 array layers!");
  const uint32_t multiplier = isCubemap_ ? static_cast<uint32_t>(arrayLayers_) / 6u : 1u;
  const uint32_t rangeStartLayer = (static_cast<uint32_t>(range.layer) * multiplier) +
                                   (isCubemap_ ? static_cast<uint32_t>(range.face) : 0u);
  const uint32_t rangeLayerCount = (static_cast<uint32_t>(range.numLayers) * multiplier) +
                                   (isCubemap_ ? static_cast<uint32_t>(range.numFaces) : 0u);

  // 0: Transition the first mip-level - all layers - to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  transitionLayout(commandBuffer,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VkImageSubresourceRange{imageAspectFlags,
                                           static_cast<uint32_t>(range.mipLevel),
                                           static_cast<uint32_t>(range.numMipLevels),
                                           rangeStartLayer,
                                           rangeLayerCount});

  for (uint32_t arrayLayer = range.layer; arrayLayer < (range.layer + range.numLayers);
       ++arrayLayer) {
    for (uint32_t face = range.face; face < (range.face + range.numFaces); ++face) {
      const uint32_t layer = arrayLayer * multiplier + face;
      auto mipWidth = extent_.width > 1 ? (int32_t)extent_.width >> (range.mipLevel) : 1;
      auto mipHeight = extent_.height > 1 ? (int32_t)extent_.height >> (range.mipLevel) : 1;

      for (uint32_t i = (range.mipLevel + 1); i < (range.mipLevel + range.numMipLevels); ++i) {
        // 1: Transition the i-th level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        //    It will be copied into from the (i-1)-th layer
        ivkImageMemoryBarrier(&ctx_->vf_,
                              commandBuffer,
                              vkImage_,
                              0, /* srcAccessMask */
                              VK_ACCESS_TRANSFER_WRITE_BIT, /* dstAccessMask */
                              VK_IMAGE_LAYOUT_UNDEFINED, /* oldImageLayout */
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /* newImageLayout */
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, /* srcStageMask */
                              VK_PIPELINE_STAGE_TRANSFER_BIT, /* dstStageMask */
                              VkImageSubresourceRange{imageAspectFlags, i, 1, layer, 1});

        const int32_t nextLevelWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        const int32_t nextLevelHeight = mipHeight > 1 ? mipHeight / 2 : 1;

        const std::array<VkOffset3D, 2> srcOffsets = {
            VkOffset3D{0, 0, 0},
            VkOffset3D{mipWidth, mipHeight, 1},
        };
        const std::array<VkOffset3D, 2> dstOffsets = {
            VkOffset3D{0, 0, 0},
            VkOffset3D{nextLevelWidth, nextLevelHeight, 1},
        };

        // 2: Blit the image from the prev mip-level (i-1) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        // to the current mip level (i) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
#if IGL_VULKAN_PRINT_COMMANDS
        IGL_LOG_INFO("%p vkCmdBlitImage()\n", commandBuffer);
#endif // IGL_VULKAN_PRINT_COMMANDS
        ivkCmdBlitImage(&ctx_->vf_,
                        commandBuffer,
                        vkImage_,
                        vkImage_,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        srcOffsets.data(),
                        dstOffsets.data(),
                        VkImageSubresourceLayers{imageAspectFlags, i - 1, layer, 1},
                        VkImageSubresourceLayers{imageAspectFlags, i, layer, 1},
                        blitFilter);

        // 3: Transition i-th level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it will be read
        // from in the next iteration
        ivkImageMemoryBarrier(&ctx_->vf_,
                              commandBuffer,
                              vkImage_,
                              VK_ACCESS_TRANSFER_WRITE_BIT, /* srcAccessMask */
                              VK_ACCESS_TRANSFER_READ_BIT, /* dstAccessMask */
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /* oldImageLayout */
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, /* newImageLayout */
                              VK_PIPELINE_STAGE_TRANSFER_BIT, /* srcStageMask */
                              VK_PIPELINE_STAGE_TRANSFER_BIT /* dstStageMask */,
                              VkImageSubresourceRange{imageAspectFlags, i, 1, layer, 1});

        // Compute the size of the next mip level
        mipWidth = nextLevelWidth;
        mipHeight = nextLevelHeight;
      }
    }
  }

  // 4: Transition all levels and layers/faces to their final layout
  ivkImageMemoryBarrier(&ctx_->vf_,
                        commandBuffer,
                        vkImage_,
                        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
                        0, // dstAccessMask
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldImageLayout
                        originalImageLayout, // newImageLayout
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
                        VkImageSubresourceRange{imageAspectFlags,
                                                static_cast<uint32_t>(range.mipLevel),
                                                static_cast<uint32_t>(range.numMipLevels),
                                                rangeStartLayer,
                                                rangeLayerCount});

  imageLayout_ = originalImageLayout;
}

bool VulkanImage::isDepthFormat(VkFormat format) {
  return (format == VK_FORMAT_D16_UNORM) || (format == VK_FORMAT_X8_D24_UNORM_PACK32) ||
         (format == VK_FORMAT_D32_SFLOAT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

bool VulkanImage::isStencilFormat(VkFormat format) {
  return (format == VK_FORMAT_S8_UINT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

void VulkanImage::setName(const std::string& name) noexcept {
#if defined(IGL_DEBUG)
  name_ = name;
#else
  (void)name;
#endif
}

bool VulkanImage::valid() const {
  return ctx_ != nullptr;
}

VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
  destroy();
  ctx_ = std::move(other.ctx_);
  physicalDevice_ = std::move(other.physicalDevice_);
  device_ = std::move(other.device_);
  vkImage_ = std::move(other.vkImage_);
  usageFlags_ = std::move(other.usageFlags_);
  vmaAllocation_ = std::move(other.vmaAllocation_);
  formatProperties_ = std::move(other.formatProperties_);
  mappedPtr_ = std::move(other.mappedPtr_);
  isExternallyManaged_ = std::move(other.isExternallyManaged_);
  extent_ = std::move(other.extent_);
  type_ = std::move(other.type_);
  imageFormat_ = std::move(other.imageFormat_);
  mipLevels_ = std::move(other.mipLevels_);
  arrayLayers_ = std::move(other.arrayLayers_);
  samples_ = std::move(other.samples_);
  isDepthFormat_ = std::move(other.isDepthFormat_);
  isStencilFormat_ = std::move(other.isStencilFormat_);
  isDepthOrStencilFormat_ = std::move(other.isDepthOrStencilFormat_);
  allocatedSize = std::move(other.allocatedSize);
  imageLayout_ = std::move(other.imageLayout_);
  isImported_ = std::move(other.isImported_);
  isCubemap_ = other.isCubemap_;
  isExported_ = other.isExported_;
  exportedMemoryHandle_ = other.exportedMemoryHandle_;
  exportedFd_ = other.exportedFd_;
#if defined(IGL_DEBUG)
  name_ = std::move(other.name_);
#endif
  tiling_ = other.tiling_;
  isCoherentMemory_ = other.isCoherentMemory_;

  for (size_t i = 0; i != IGL_ARRAY_NUM_ELEMENTS(vkMemory_); i++) {
    vkMemory_[i] = other.vkMemory_[i];
  }

  other.ctx_ = nullptr;
  other.vkImage_ = VK_NULL_HANDLE;

  return *this;
}

void VulkanImage::flushMappedMemory() const {
  if (!isMappedPtrAccessible() || isCoherentMemory()) {
    return;
  }

  if (vmaAllocation_) {
    vmaFlushAllocation((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, 0, VK_WHOLE_SIZE);
  } else {
    const VkMappedMemoryRange memoryRange{
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        nullptr,
        vkMemory_[0],
        0,
        VK_WHOLE_SIZE,
    };
    ctx_->vf_.vkFlushMappedMemoryRanges(device_, 1, &memoryRange);
  }
}

} // namespace igl::vulkan
