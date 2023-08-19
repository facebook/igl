/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanClasses.h"
#include "VulkanUtils.h"

#include <igl/vulkan/VulkanContext.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#include <unistd.h>
#endif

lvk::VulkanBuffer::VulkanBuffer(lvk::vulkan::VulkanContext* ctx,
                                VkDevice device,
                                VkDeviceSize bufferSize,
                                VkBufferUsageFlags usageFlags,
                                VkMemoryPropertyFlags memFlags,
                                const char* debugName) :
  ctx_(ctx), device_(device), bufferSize_(bufferSize), vkUsageFlags_(usageFlags), vkMemFlags_(memFlags) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT(ctx);
  LVK_ASSERT(bufferSize > 0);

  const VkBufferCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = bufferSize,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };

  if (LVK_VULKAN_USE_VMA) {
    // Initialize VmaAllocation Info
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaAllocInfo_.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      vmaAllocInfo_.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
      vmaAllocInfo_.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }
    if (memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
      vmaAllocInfo_.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    vmaAllocInfo_.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer((VmaAllocator)ctx_->getVmaAllocator(), &ci, &vmaAllocInfo_, &vkBuffer_, &vmaAllocation_, nullptr);

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, &mappedPtr_);
    }
  } else {
    // create buffer
    VK_ASSERT(vkCreateBuffer(device_, &ci, nullptr, &vkBuffer_));

    // back the buffer with some memory
    {
      VkMemoryRequirements requirements = {};
      vkGetBufferMemoryRequirements(device_, vkBuffer_, &requirements);

      VK_ASSERT(lvk::allocateMemory(ctx_->getVkPhysicalDevice(), device_, &requirements, memFlags, &vkMemory_));
      VK_ASSERT(vkBindBufferMemory(device_, vkBuffer_, vkMemory_, 0));
    }

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_ASSERT(vkMapMemory(device_, vkMemory_, 0, bufferSize_, 0, &mappedPtr_));
    }
  }

  LVK_ASSERT(vkBuffer_ != VK_NULL_HANDLE);

  // set debug name
  VK_ASSERT(lvk::setDebugObjectName(device_, VK_OBJECT_TYPE_BUFFER, (uint64_t)vkBuffer_, debugName));

  // handle shader access
  if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    const VkBufferDeviceAddressInfo ai = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, vkBuffer_};
    vkDeviceAddress_ = vkGetBufferDeviceAddress(device_, &ai);
    LVK_ASSERT(vkDeviceAddress_);
  }
}

lvk::VulkanBuffer::~VulkanBuffer() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (!ctx_) {
    return;
  }

  if (LVK_VULKAN_USE_VMA) {
    if (mappedPtr_) {
      vmaUnmapMemory((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_);
    }
    ctx_->deferredTask(std::packaged_task<void()>([vma = ctx_->getVmaAllocator(), buffer = vkBuffer_, allocation = vmaAllocation_]() {
      vmaDestroyBuffer((VmaAllocator)vma, buffer, allocation);
    }));
  } else {
    if (mappedPtr_) {
      vkUnmapMemory(device_, vkMemory_);
    }
    ctx_->deferredTask(std::packaged_task<void()>([device = device_, buffer = vkBuffer_, memory = vkMemory_]() {
      vkDestroyBuffer(device, buffer, nullptr);
      vkFreeMemory(device, memory, nullptr);
    }));
  }
}

lvk::VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) :
  ctx_(other.ctx_),
  device_(other.device_),
  vkBuffer_(other.vkBuffer_),
  vkMemory_(other.vkMemory_),
  vmaAllocInfo_(other.vmaAllocInfo_),
  vmaAllocation_(other.vmaAllocation_),
  vkDeviceAddress_(other.vkDeviceAddress_),
  bufferSize_(other.bufferSize_),
  vkUsageFlags_(other.vkUsageFlags_),
  vkMemFlags_(other.vkMemFlags_),
  mappedPtr_(other.mappedPtr_) {
  other.ctx_ = nullptr;
}

lvk::VulkanBuffer& lvk::VulkanBuffer::operator=(VulkanBuffer&& other) {
  std::swap(ctx_, other.ctx_);
  std::swap(device_, other.device_);
  std::swap(vkBuffer_, other.vkBuffer_);
  std::swap(vkMemory_, other.vkMemory_);
  std::swap(vmaAllocInfo_, other.vmaAllocInfo_);
  std::swap(vmaAllocation_, other.vmaAllocation_);
  std::swap(vkDeviceAddress_, other.vkDeviceAddress_);
  std::swap(bufferSize_, other.bufferSize_);
  std::swap(vkUsageFlags_, other.vkUsageFlags_);
  std::swap(vkMemFlags_, other.vkMemFlags_);
  std::swap(mappedPtr_, other.mappedPtr_);
  return *this;
}

void lvk::VulkanBuffer::flushMappedMemory(VkDeviceSize offset, VkDeviceSize size) const {
  if (!LVK_VERIFY(isMapped())) {
    return;
  }

  if (LVK_VULKAN_USE_VMA) {
    vmaFlushAllocation((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, offset, size);
  } else {
    const VkMappedMemoryRange memoryRange = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        nullptr,
        vkMemory_,
        offset,
        size,
    };
    vkFlushMappedMemoryRanges(device_, 1, &memoryRange);
  }
}

void lvk::VulkanBuffer::getBufferSubData(size_t offset, size_t size, void* data) {
  // only host-visible buffers can be downloaded this way
  LVK_ASSERT(mappedPtr_);

  if (!mappedPtr_) {
    return;
  }

  LVK_ASSERT(offset + size <= bufferSize_);

  const uint8_t* src = static_cast<uint8_t*>(mappedPtr_) + offset;
  memcpy(data, src, size);
}

void lvk::VulkanBuffer::bufferSubData(size_t offset, size_t size, const void* data) {
  // only host-visible buffers can be uploaded this way
  LVK_ASSERT(mappedPtr_);

  if (!mappedPtr_) {
    return;
  }

  LVK_ASSERT(offset + size <= bufferSize_);

  if (data) {
    memcpy((uint8_t*)mappedPtr_ + offset, data, size);
  } else {
    memset((uint8_t*)mappedPtr_ + offset, 0, size);
  }
}

lvk::VulkanImage::VulkanImage(lvk::vulkan::VulkanContext& ctx,
                              VkDevice device,
                              VkImage image,
                              VkImageUsageFlags usageFlags,
                              VkFormat imageFormat,
                              VkExtent3D extent,
                              const char* debugName) :
  ctx_(ctx),
  vkPhysicalDevice_(ctx.getVkPhysicalDevice()),
  vkDevice_(device),
  vkImage_(image),
  vkUsageFlags_(usageFlags),
  isSwapchainImage_(true),
  vkExtent_(extent),
  vkType_(VK_IMAGE_TYPE_2D),
  vkImageFormat_(imageFormat),
  isDepthFormat_(isDepthFormat(imageFormat)),
  isStencilFormat_(isStencilFormat(imageFormat)) {
  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));
}

lvk::VulkanImage::VulkanImage(lvk::vulkan::VulkanContext& ctx,
                              VkDevice device,
                              VkExtent3D extent,
                              VkImageType type,
                              VkFormat format,
                              uint32_t numLevels,
                              uint32_t numLayers,
                              VkImageTiling tiling,
                              VkImageUsageFlags usageFlags,
                              VkMemoryPropertyFlags memFlags,
                              VkImageCreateFlags createFlags,
                              VkSampleCountFlagBits samples,
                              const char* debugName) :
  ctx_(ctx),
  vkPhysicalDevice_(ctx.getVkPhysicalDevice()),
  vkDevice_(device),
  vkUsageFlags_(usageFlags),
  vkExtent_(extent),
  vkType_(type),
  vkImageFormat_(format),
  numLevels_(numLevels),
  numLayers_(numLayers),
  vkSamples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT_MSG(numLevels_ > 0, "The image must contain at least one mip-level");
  LVK_ASSERT_MSG(numLayers_ > 0, "The image must contain at least one layer");
  LVK_ASSERT_MSG(vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  LVK_ASSERT_MSG(vkSamples_ > 0, "The image must contain at least one sample");
  LVK_ASSERT(extent.width > 0);
  LVK_ASSERT(extent.height > 0);
  LVK_ASSERT(extent.depth > 0);

  const VkImageCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = createFlags,
      .imageType = type,
      .format = vkImageFormat_,
      .extent = vkExtent_,
      .mipLevels = numLevels_,
      .arrayLayers = numLayers_,
      .samples = samples,
      .tiling = tiling,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  if (LVK_VULKAN_USE_VMA) {
    vmaAllocInfo_.usage = memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_AUTO;
    VkResult result = vmaCreateImage((VmaAllocator)ctx_.getVmaAllocator(), &ci, &vmaAllocInfo_, &vkImage_, &vmaAllocation_, nullptr);

    if (!LVK_VERIFY(result == VK_SUCCESS)) {
      LLOGW("failed: error result: %d, memflags: %d,  imageformat: %d\n", result, memFlags, vkImageFormat_);
    }

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_, &mappedPtr_);
    }
  } else {
    // create image
    VK_ASSERT(vkCreateImage(vkDevice_, &ci, nullptr, &vkImage_));

    // back the image with some memory
    {
      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(device, vkImage_, &memRequirements);

      VK_ASSERT(lvk::allocateMemory(vkPhysicalDevice_, vkDevice_, &memRequirements, memFlags, &vkMemory_));
      VK_ASSERT(vkBindImageMemory(vkDevice_, vkImage_, vkMemory_, 0));
    }

    // handle memory-mapped images
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_ASSERT(vkMapMemory(vkDevice_, vkMemory_, 0, VK_WHOLE_SIZE, 0, &mappedPtr_));
    }
  }

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  // Get physical device's properties for the image's format
  vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_, vkImageFormat_, &vkFormatProperties_);
}

lvk::VulkanImage::~VulkanImage() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (!isSwapchainImage_) {
    if (LVK_VULKAN_USE_VMA) {
      if (mappedPtr_) {
        vmaUnmapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_);
      }
      ctx_.deferredTask(std::packaged_task<void()>([vma = ctx_.getVmaAllocator(), image = vkImage_, allocation = vmaAllocation_]() {
        vmaDestroyImage((VmaAllocator)vma, image, allocation);
      }));
    } else {
      if (mappedPtr_) {
        vkUnmapMemory(vkDevice_, vkMemory_);
      }
      ctx_.deferredTask(std::packaged_task<void()>([device = vkDevice_, image = vkImage_, memory = vkMemory_]() {
        vkDestroyImage(device, image, nullptr);
        if (memory != VK_NULL_HANDLE) {
          vkFreeMemory(device, memory, nullptr);
        }
      }));
    }
  }
}

VkImageView lvk::VulkanImage::createImageView(VkImageViewType type,
                                              VkFormat format,
                                              VkImageAspectFlags aspectMask,
                                              uint32_t baseLevel,
                                              uint32_t numLevels,
                                              uint32_t baseLayer,
                                              uint32_t numLayers,
                                              const char* debugName) const {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  const VkImageViewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vkImage_,
      .viewType = type,
      .format = format,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {aspectMask, baseLevel, numLevels ? numLevels : numLevels_, baseLayer, numLayers},
  };
  VkImageView vkView = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateImageView(vkDevice_, &ci, nullptr, &vkView));
  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkView, debugName));

  return vkView;
}

void lvk::VulkanImage::transitionLayout(VkCommandBuffer commandBuffer,
                                        VkImageLayout newImageLayout,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask,
                                        const VkImageSubresourceRange& subresourceRange) const {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_TRANSITION);

  VkAccessFlags srcAccessMask = 0;
  VkAccessFlags dstAccessMask = 0;

  if (vkImageLayout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
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
    LVK_ASSERT_MSG(false, "Automatic access mask deduction is not implemented (yet) for this srcStageMask");
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
    LVK_ASSERT_MSG(false, "Automatic access mask deduction is not implemented (yet) for this dstStageMask");
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

  lvk::imageMemoryBarrier(
      commandBuffer, vkImage_, srcAccessMask, dstAccessMask, vkImageLayout_, newImageLayout, srcStageMask, dstStageMask, subresourceRange);

  vkImageLayout_ = newImageLayout;
}

VkImageAspectFlags lvk::VulkanImage::getImageAspectFlags() const {
  VkImageAspectFlags flags = 0;

  flags |= isDepthFormat_ ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
  flags |= isStencilFormat_ ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
  flags |= !(isDepthFormat_ || isStencilFormat_) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;

  return flags;
}

void lvk::VulkanImage::generateMipmap(VkCommandBuffer commandBuffer) const {
  LVK_PROFILER_FUNCTION();

  // Check if device supports downscaling for color or depth/stencil buffer based on image format
  {
    const uint32_t formatFeatureMask = (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

    const bool hardwareDownscalingSupported = (vkFormatProperties_.optimalTilingFeatures & formatFeatureMask) == formatFeatureMask;

    if (!LVK_VERIFY(hardwareDownscalingSupported)) {
      LVK_ASSERT_MSG(false, "Doesn't support hardware downscaling of this image format: {}");
      return;
    }
  }

  // Choose linear filter for color formats if supported by the device, else use nearest filter
  // Choose nearest filter by default for depth/stencil formats
  const VkFilter blitFilter = [](bool isDepthOrStencilFormat, bool imageFilterLinear) {
    if (isDepthOrStencilFormat) {
      return VK_FILTER_NEAREST;
    }
    if (imageFilterLinear) {
      return VK_FILTER_LINEAR;
    }
    return VK_FILTER_NEAREST;
  }(isDepthFormat_ || isStencilFormat_, vkFormatProperties_.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  const VkImageAspectFlags imageAspectFlags = getImageAspectFlags();

  const VkDebugUtilsLabelEXT utilsLabel = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = "Generate mipmaps",
      .color = {1.0f, 0.75f, 1.0f, 1.0f},
  };
  vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &utilsLabel);

  const VkImageLayout originalImageLayout = vkImageLayout_;

  LVK_ASSERT(originalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

  // 0: Transition the first level and all layers into VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  transitionLayout(commandBuffer,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, numLayers_});

  for (uint32_t layer = 0; layer < numLayers_; ++layer) {
    int32_t mipWidth = (int32_t)vkExtent_.width;
    int32_t mipHeight = (int32_t)vkExtent_.height;

    for (uint32_t i = 1; i < numLevels_; ++i) {
      // 1: Transition the i-th level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; it will be copied into from the (i-1)-th layer
      lvk::imageMemoryBarrier(commandBuffer,
                              vkImage_,
                              0, /* srcAccessMask */
                              VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
                              VK_IMAGE_LAYOUT_UNDEFINED, // oldImageLayout
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask
                              VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
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

      // 2: Blit the image from the prev mip-level (i-1) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) to the current mip-level (i)
      // (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
#if LVK_VULKAN_PRINT_COMMANDS
      LLOGL("%p vkCmdBlitImage()\n", commandBuffer);
#endif // LVK_VULKAN_PRINT_COMMANDS
      const VkImageBlit blit = {
          .srcSubresource = VkImageSubresourceLayers{imageAspectFlags, i - 1, layer, 1},
          .srcOffsets = {srcOffsets[0], srcOffsets[1]},
          .dstSubresource = VkImageSubresourceLayers{imageAspectFlags, i, layer, 1},
          .dstOffsets = {dstOffsets[0], dstOffsets[1]},
      };
      vkCmdBlitImage(commandBuffer,
                     vkImage_,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     vkImage_,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     1,
                     &blit,
                     blitFilter);
      // 3: Transition i-th level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it will be read from in
      // the next iteration
      lvk::imageMemoryBarrier(commandBuffer,
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

  // 4: Transition all levels and layers (faces) to their final layout
  lvk::imageMemoryBarrier(commandBuffer,
                          vkImage_,
                          VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
                          0, // dstAccessMask
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldImageLayout
                          originalImageLayout, // newImageLayout
                          VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
                          VkImageSubresourceRange{imageAspectFlags, 0, numLevels_, 0, numLayers_});
  vkCmdEndDebugUtilsLabelEXT(commandBuffer);

  vkImageLayout_ = originalImageLayout;
}

bool lvk::VulkanImage::isDepthFormat(VkFormat format) {
  return (format == VK_FORMAT_D16_UNORM) || (format == VK_FORMAT_X8_D24_UNORM_PACK32) || (format == VK_FORMAT_D32_SFLOAT) ||
         (format == VK_FORMAT_D16_UNORM_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

bool lvk::VulkanImage::isStencilFormat(VkFormat format) {
  return (format == VK_FORMAT_S8_UINT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

lvk::VulkanTexture::VulkanTexture(std::shared_ptr<VulkanImage> image, VkImageView imageView) : image_(std::move(image)), imageView_(imageView) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT(image_.get());
  LVK_ASSERT(imageView_ != VK_NULL_HANDLE);
}

lvk::VulkanTexture::~VulkanTexture() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (image_) {
    image_->ctx_.deferredTask(std::packaged_task<void()>(
        [device = image_->ctx_.getVkDevice(), imageView = imageView_]() { vkDestroyImageView(device, imageView, nullptr); }));
    for (VkImageView v : imageViewForFramebuffer_) {
      if (v != VK_NULL_HANDLE) {
        image_->ctx_.deferredTask(std::packaged_task<void()>(
            [device = image_->ctx_.getVkDevice(), imageView = v]() { vkDestroyImageView(device, imageView, nullptr); }));
      }
    }
  }
}

lvk::VulkanTexture::VulkanTexture(VulkanTexture&& other) {
  std::swap(image_, other.image_);
  std::swap(imageView_, other.imageView_);
  for (size_t i = 0; i != LVK_MAX_MIP_LEVELS; i++) {
    std::swap(imageViewForFramebuffer_[i], other.imageViewForFramebuffer_[i]);
  }
}

lvk::VulkanTexture& lvk::VulkanTexture::operator=(VulkanTexture&& other) {
  std::swap(image_, other.image_);
  std::swap(imageView_, other.imageView_);
  for (size_t i = 0; i != LVK_MAX_MIP_LEVELS; i++) {
    std::swap(imageViewForFramebuffer_[i], other.imageViewForFramebuffer_[i]);
  }
  return *this;
}

VkImageView lvk::VulkanTexture::getOrCreateVkImageViewForFramebuffer(uint8_t level) {
  LVK_ASSERT(image_ != nullptr);
  LVK_ASSERT(level < LVK_MAX_MIP_LEVELS);

  if (!image_ || level >= LVK_MAX_MIP_LEVELS) {
    return VK_NULL_HANDLE;
  }

  if (imageViewForFramebuffer_[level] != VK_NULL_HANDLE) {
    return imageViewForFramebuffer_[level];
  }

  imageViewForFramebuffer_[level] =
      image_->createImageView(VK_IMAGE_VIEW_TYPE_2D, image_->vkImageFormat_, image_->getImageAspectFlags(), level, 1u, 0u, 1u);

  return imageViewForFramebuffer_[level];
}
