/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImage.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImageView.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#include <unistd.h>
#endif

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
  LLOGW("Memory type %d with properties %d not found.", typeBits, requiredProperties);
  return 0;
}

// VkImage export and import is only implemented on Windows, Linux and Android platforms.
#if IGL_PLATFORM_WIN
constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
#elif IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

} // namespace

namespace igl {

namespace vulkan {

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
  ctx_(ctx),
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
  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));
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
                         const char* debugName) :
  ctx_(ctx),
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
  isImported_(false) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT_MSG(mipLevels_ > 0, "The image must contain at least one mip level");
  IGL_ASSERT_MSG(arrayLayers_ > 0, "The image must contain at least one layer");
  IGL_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  IGL_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");

  const VkImageCreateInfo ci = ivkGetImageCreateInfo(type,
                                                     imageFormat_,
                                                     tiling,
                                                     usageFlags,
                                                     extent_,
                                                     mipLevels_,
                                                     arrayLayers_,
                                                     createFlags,
                                                     samples);

  if (IGL_VULKAN_USE_VMA) {
    vmaAllocInfo_.usage = memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                              ? VMA_MEMORY_USAGE_CPU_TO_GPU
                              : VMA_MEMORY_USAGE_AUTO;

    VkResult result = vmaCreateImage((VmaAllocator)ctx_.getVmaAllocator(),
                                     &ci,
                                     &vmaAllocInfo_,
                                     &vkImage_,
                                     &vmaAllocation_,
                                     nullptr);

    if (!IGL_VERIFY(result == VK_SUCCESS)) {
      LLOGW("failed: error result: %d, memflags: %d,  imageformat: %d\n",
                    result,
                    memFlags,
                    imageFormat_);
    }

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_, &mappedPtr_);
    }

    if (vmaAllocation_) {
      VmaAllocationInfo allocationInfo;
      vmaGetAllocationInfo((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_, &allocationInfo);
      allocatedSize = allocationInfo.size;
    }
  } else {
    // create image
    VK_ASSERT(vkCreateImage(device_, &ci, nullptr, &vkImage_));

    // back the image with some memory
    {
      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(device, vkImage_, &memRequirements);

      VK_ASSERT(
          ivkAllocateMemory(physicalDevice_, device_, &memRequirements, memFlags, &vkMemory_));
      VK_ASSERT(vkBindImageMemory(device_, vkImage_, vkMemory_, 0));

      allocatedSize = memRequirements.size;
    }

    // handle memory-mapped images
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_ASSERT(vkMapMemory(device_, vkMemory_, 0, VK_WHOLE_SIZE, 0, &mappedPtr_));
    }
  }

  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  // Get physical device's properties for the image's format
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, imageFormat_, &formatProperties_);
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
  ctx_(ctx),
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
  isImported_(true) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ASSERT_MSG(mipLevels_ > 0, "The image must contain at least one mip level");
  IGL_ASSERT_MSG(arrayLayers_ > 0, "The image must contain at least one layer");
  IGL_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  IGL_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");

#ifdef VK_USE_PLATFORM_WIN32_KHR
  IGL_ASSERT_MSG(false, "You can only import a VulkanImage on non-windows environments");
#endif

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
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.queueFamilyIndexCount = 0;
  ci.pQueueFamilyIndices = nullptr;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &vulkanMemoryProperties);

  // create image.. importing external memory cannot use VMA
  VK_ASSERT(vkCreateImage(device_, &ci, nullptr, &vkImage_));
  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

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

  VkImageMemoryRequirementsInfo2 memoryRequirementInfo = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, vkImage_};

  VkMemoryRequirements2 memoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  vkGetImageMemoryRequirements2KHR(device_, &memoryRequirementInfo, &memoryRequirements);

  // TODO_VULKAN: Verify the following from the spec:
  // the memory from which fd was exported must have been created on the same physical device
  // as device.
  VkImportMemoryFdInfoKHR fdInfo = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                    nullptr,
                                    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
                                    importedFd};

  VkMemoryAllocateInfo memoryAllocateInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &fdInfo,
      memoryAllocationSize,
      ivkGetMemoryTypeIndex(vulkanMemoryProperties,
                            memoryRequirements.memoryRequirements.memoryTypeBits,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

  LLOGL("Imported texture has requirements %d, ends up index %d",
               memoryRequirements.memoryRequirements.memoryTypeBits,
               memoryAllocateInfo.memoryTypeIndex);

  VK_ASSERT(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vkMemory_));
  VK_ASSERT(vkBindImageMemory(device_, vkImage_, vkMemory_, 0));
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
  ctx_(ctx),
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
  isImported_(true) {
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
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.queueFamilyIndexCount = 0;
  ci.pQueueFamilyIndices = nullptr;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &vulkanMemoryProperties);

  // create image.. importing external memory cannot use VMA
  VK_ASSERT(vkCreateImage(device_, &ci, nullptr, &vkImage_));
  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  const VkImageMemoryRequirementsInfo2 memoryRequirementInfo = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, vkImage_};

  VkMemoryRequirements2 memoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  vkGetImageMemoryRequirements2KHR(device_, &memoryRequirementInfo, &memoryRequirements);

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

  VK_ASSERT(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vkMemory_));
  VK_ASSERT(vkBindImageMemory(device_, vkImage_, vkMemory_, 0));
}
#endif // IGL_PLATFORM_WIN

#if IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
std::shared_ptr<VulkanImage> VulkanImage::createWithExportMemory(const VulkanContext& ctx,
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
                                                                 const char* debugName) {
  const VkPhysicalDeviceExternalImageFormatInfo externaInfo = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
      nullptr,
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
  const auto result = vkGetPhysicalDeviceImageFormatProperties2(
      ctx.getVkPhysicalDevice(), &formatInfo2, &imageFormatProperties2);
  if (result != VK_SUCCESS) {
    return nullptr;
  }
  const auto& externalFormatProperties = externalImageFormatProperties.externalMemoryProperties;
  if (!(externalFormatProperties.externalMemoryFeatures &
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
    return nullptr;
  }
  const auto compatibleHandleTypes = externalFormatProperties.compatibleHandleTypes;
  IGL_ASSERT(compatibleHandleTypes & kHandleType);
  const VkExternalMemoryImageCreateInfoKHR externalImageCreateInfo = {
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      nullptr,
      compatibleHandleTypes,
  };
  const VkExportMemoryAllocateInfoKHR externalMemoryAllocateInfo = {
      VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
      nullptr,
      compatibleHandleTypes,
  };
  return std::shared_ptr<VulkanImage>(new VulkanImage(ctx,
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
                                                      externalImageCreateInfo,
                                                      externalMemoryAllocateInfo,
                                                      debugName));
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
                         const VkExternalMemoryImageCreateInfoKHR& externalImageCreateInfo,
                         const VkExportMemoryAllocateInfoKHR& externalMemoryAllocateInfo,
                         const char* debugName) :
  ctx_(ctx),
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
  isExported_(true) {
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

  ci.pNext = &externalImageCreateInfo;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.queueFamilyIndexCount = 0;
  ci.pQueueFamilyIndices = nullptr;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkPhysicalDeviceMemoryProperties vulkanMemoryProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &vulkanMemoryProperties);

  // create image.. importing external memory cannot use VMA
  VK_ASSERT(vkCreateImage(device_, &ci, nullptr, &vkImage_));
  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  const VkImageMemoryRequirementsInfo2 memoryRequirementInfo = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, vkImage_};

  VkMemoryRequirements2 memoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
  vkGetImageMemoryRequirements2KHR(device_, &memoryRequirementInfo, &memoryRequirements);

  const VkMemoryAllocateInfo memoryAllocateInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &externalMemoryAllocateInfo,
      memoryRequirements.memoryRequirements.size,
      ivkGetMemoryTypeIndex(
          vulkanMemoryProperties, memoryRequirements.memoryRequirements.memoryTypeBits, memFlags)};

  VK_ASSERT(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vkMemory_));
  VK_ASSERT(vkBindImageMemory(device_, vkImage_, vkMemory_, 0));

#if IGL_PLATFORM_WIN
  const VkMemoryGetWin32HandleInfoKHR getHandleInfo{
      VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR, nullptr, vkMemory_, kHandleType};
  VK_ASSERT(vkGetMemoryWin32HandleKHR(device_, &getHandleInfo, &exportedMemoryHandle_));
#else
  VkMemoryGetFdInfoKHR getFdInfo{.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
                                 .pNext = nullptr,
                                 .memory = vkMemory_,
                                 .handleType = kHandleType};
  VK_ASSERT(vkGetMemoryFdKHR(device_, &getFdInfo, &exportedFd_));
#endif
}
#endif // IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID

VulkanImage::~VulkanImage() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (!isExternallyManaged_) {
    if (IGL_VULKAN_USE_VMA && !isImported_ && !isExported_) {
      if (mappedPtr_) {
        vmaUnmapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_);
      }
      ctx_.deferredTask(std::packaged_task<void()>(
          [vma = ctx_.getVmaAllocator(), image = vkImage_, allocation = vmaAllocation_]() {
            vmaDestroyImage((VmaAllocator)vma, image, allocation);
          }));
    } else {
      if (mappedPtr_) {
        vkUnmapMemory(device_, vkMemory_);
      }
      ctx_.deferredTask(
          std::packaged_task<void()>([device = device_, image = vkImage_, memory = vkMemory_]() {
            vkDestroyImage(device, image, nullptr);
            if (memory != VK_NULL_HANDLE) {
              vkFreeMemory(device, memory, nullptr);
            }
          }));
    }
  }
}

std::shared_ptr<VulkanImageView> VulkanImage::createImageView(VkImageViewType type,
                                                              VkFormat format,
                                                              VkImageAspectFlags aspectMask,
                                                              uint32_t baseLevel,
                                                              uint32_t numLevels,
                                                              uint32_t baseLayer,
                                                              uint32_t numLayers,
                                                              const char* debugName) const {
  return std::make_shared<VulkanImageView>(ctx_,
                                           device_,
                                           vkImage_,
                                           type,
                                           format,
                                           aspectMask,
                                           baseLevel,
                                           numLevels ? numLevels : mipLevels_,
                                           baseLayer,
                                           numLayers,
                                           debugName);
}

void VulkanImage::transitionLayout(VkCommandBuffer commandBuffer,
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

  switch (srcStageMask) {
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
  case VK_PIPELINE_STAGE_TRANSFER_BIT:
  case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
    break;
  default:
    IGL_ASSERT_MSG(
        false, "Automatic access mask deduction is not implemented (yet) for this srcStageMask");
    break;
  }

  // once you want to add a new pipeline stage to this block of if's, don't forget to add it to the
  // switch() statement above
  if (srcStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
  }

  switch (dstStageMask) {
  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
  case VK_PIPELINE_STAGE_TRANSFER_BIT:
  case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
    break;
  default:
    IGL_ASSERT_MSG(
        false, "Automatic access mask deduction is not implemented (yet) for this dstStageMask");
    break;
  }

  // once you want to add a new pipeline stage to this block of if's, don't forget to add it to the
  // switch() statement above
  if (dstStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
  }

  ivkImageMemoryBarrier(commandBuffer,
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

void VulkanImage::generateMipmap(VkCommandBuffer commandBuffer) const {
  IGL_PROFILER_FUNCTION();

  // Check if device supports downscaling for color or depth/stancil buffer based on image format
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
        formatProperties_.optimalTilingFeatures &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  const VkImageAspectFlags imageAspectFlags = getImageAspectFlags();

  ivkCmdBeginDebugUtilsLabel(
      commandBuffer, "Generate mipmaps", igl::Color(1.f, 0.75f, 0.f).toFloatPtr());
  SCOPE_EXIT {
    ivkCmdEndDebugUtilsLabel(commandBuffer);
  };

  const VkImageLayout originalImageLayout = imageLayout_;

  IGL_ASSERT(originalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

  // 0: Transition the first mip-level - all layers - to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  transitionLayout(commandBuffer,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, arrayLayers_});

  for (uint32_t layer = 0; layer < arrayLayers_; ++layer) {
    auto mipWidth = (int32_t)extent_.width;
    auto mipHeight = (int32_t)extent_.height;

    for (uint32_t i = 1; i < mipLevels_; ++i) {
      // 1: Transition the i-th level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
      //    It will be copied into from the (i-1)-th layer
      ivkImageMemoryBarrier(commandBuffer,
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

      const VkOffset3D srcOffsets[2] = {
          VkOffset3D{0, 0, 0},
          VkOffset3D{mipWidth, mipHeight, 1},
      };
      const VkOffset3D dstOffsets[2] = {
          VkOffset3D{0, 0, 0},
          VkOffset3D{nextLevelWidth, nextLevelHeight, 1},
      };

      // 2: Blit the image from the prev mip-level (i-1) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) to
      // the current mip level (i) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
#if IGL_VULKAN_PRINT_COMMANDS
      IGL_LOG_INFO("%p vkCmdBlitImage()\n", commandBuffer);
#endif // IGL_VULKAN_PRINT_COMMANDS
      ivkCmdBlitImage(commandBuffer,
                      vkImage_,
                      vkImage_,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      srcOffsets,
                      dstOffsets,
                      VkImageSubresourceLayers{imageAspectFlags, i - 1, layer, 1},
                      VkImageSubresourceLayers{imageAspectFlags, i, layer, 1},
                      blitFilter);

      // 3: Transition i-th level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it will be read from in
      // the next iteration
      ivkImageMemoryBarrier(commandBuffer,
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

  // 4: Transition all levels and layers/faces to their final layout
  ivkImageMemoryBarrier(commandBuffer,
                        vkImage_,
                        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
                        0, // dstAccessMask
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldImageLayout
                        originalImageLayout, // newImageLayout
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
                        VkImageSubresourceRange{imageAspectFlags, 0, mipLevels_, 0, arrayLayers_});

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

} // namespace vulkan

} // namespace igl
