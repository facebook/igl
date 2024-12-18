/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanStagingDevice.h>

#include <igl/IGLSafeC.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

#define IGL_VULKAN_DEBUG_STAGING_DEVICE 0

using VulkanSubmitHandle = igl::vulkan::VulkanImmediateCommands::SubmitHandle;

constexpr VkDeviceSize kMinStagingBufferSize = static_cast<const VkDeviceSize>(1024u) * 1024u;

namespace igl::vulkan {

VulkanStagingDevice::VulkanStagingDevice(VulkanContext& ctx) : ctx_(ctx) {
  IGL_PROFILER_FUNCTION();

  const auto& limits = ctx_.getVkPhysicalDeviceProperties().limits;

  // Use value of 256MB (limited by some architectures), and clamp it to the max limits
  maxStagingBufferSize_ = std::min(limits.maxStorageBufferRange, 256u * 1024u * 1024u);

  immediate_ = std::make_unique<igl::vulkan::VulkanImmediateCommands>(
      ctx_.vf_,
      ctx_.device_->getVkDevice(),
      ctx_.deviceQueues_.graphicsQueueFamilyIndex,
      ctx_.config_.exportableFences,
      "VulkanStagingDevice::immediate_");
  IGL_DEBUG_ASSERT(immediate_.get());
}

void VulkanStagingDevice::bufferSubData(VulkanBuffer& buffer,
                                        size_t dstOffset,
                                        size_t size,
                                        const void* data) {
  IGL_PROFILER_FUNCTION();
  if (buffer.isMapped()) {
    buffer.bufferSubData(dstOffset, size, data);
    return;
  }

  uint32_t chunkDstOffset = dstOffset;
  void* copyData = const_cast<void*>(data);

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Upload requested for data with %u bytes\n", size);
#endif

  while (size) {
    // finds a free memory block to store the data in the staging buffer
    MemoryRegion memoryChunk = nextFreeBlock(size, false);
    const VkDeviceSize copySize = std::min(static_cast<VkDeviceSize>(size), memoryChunk.size);

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
    IGL_LOG_INFO("\tUploading %u bytes\n", copySize);
#endif

    auto& stagingBuffer = stagingBuffers_[memoryChunk.stagingBufferIndex];

    // copy data into the staging buffer
    stagingBuffer->bufferSubData(memoryChunk.offset, copySize, copyData);

    // do the transfer
    const VkBufferCopy copy = {memoryChunk.offset, chunkDstOffset, copySize};

    const auto& wrapper = immediate_->acquire();
    ctx_.vf_.vkCmdCopyBuffer(
        wrapper.cmdBuf_, stagingBuffer->getVkBuffer(), buffer.getVkBuffer(), 1, &copy);
    memoryChunk.handle = immediate_->submit(wrapper); // store the submit handle with the allocation
    regions_.push_back(memoryChunk);

    size -= copySize;
    copyData = (uint8_t*)copyData + copySize;
    chunkDstOffset += copySize;
  }
}

void VulkanStagingDevice::mergeRegionsAndFreeBuffers() {
  uint32_t regionIndex = 0;
  while (regionIndex < regions_.size() && immediate_->isReady(regions_[regionIndex].handle)) {
    auto& currRegion = regions_[regionIndex];

    // set empty handle for a region, if it has finished processing
    // so handle.empty() check can be done later
    if (!currRegion.handle.empty() && immediate_->isReady(currRegion.handle)) {
      currRegion.handle = VulkanImmediateCommands::SubmitHandle();
      freeStagingBufferSize_ += currRegion.size;
    }

    // if a next region exist and is not busy
    if ((regionIndex + 1) < regions_.size() && regions_[regionIndex + 1].handle.empty()) {
      auto& nextRegion = regions_[regionIndex + 1];
      // if current and next region are parts of the same buffer
      if (currRegion.stagingBufferIndex == nextRegion.stagingBufferIndex) {
        // if current and next region are adjacent memory blocks, merge them into one
        const bool adjacentRegions = (currRegion.size + currRegion.offset) == nextRegion.offset ||
                                     (nextRegion.size + nextRegion.offset) == currRegion.offset;

        if (adjacentRegions) {
          const MemoryRegion newRegion = {std::min(currRegion.offset, nextRegion.offset),
                                          currRegion.size + nextRegion.size,
                                          currRegion.alignedSize,
                                          VulkanImmediateCommands::SubmitHandle(),
                                          currRegion.stagingBufferIndex};
          nextRegion = newRegion;
          regions_.erase(regions_.begin() + regionIndex);
          continue;
        }
      } else {
        // if current and next region are not parts of the same buffer
        // move region with smaller staging buffer index first, so regions would eventually line up
        // if they have been split
        if (currRegion.stagingBufferIndex > nextRegion.stagingBufferIndex) {
          std::swap(currRegion, nextRegion);
        }
      }
    }

    // if a staging buffer is completely recovered
    if (currRegion.size == currRegion.alignedSize) {
      freeStagingBufferSize_ -= currRegion.size;
      // free the staging buffer
      stagingBuffers_[currRegion.stagingBufferIndex].reset();
      // remove the region
      regions_.erase(regions_.begin() + regionIndex);

      // remove trailing empty staging buffers
      while (!stagingBuffers_.empty() && stagingBuffers_.back().get() == nullptr) {
        stagingBuffers_.pop_back();
      }
      continue;
    }

    regionIndex++;
  }

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Regions: %d Staging buffers: %d Free space: %d\n",
               regions_.size(),
               stagingBuffers_.size(),
               freeStagingBufferSize_);
#endif
}

VulkanStagingDevice::MemoryRegion VulkanStagingDevice::nextFreeBlock(VkDeviceSize size,
                                                                     bool contiguous) {
  IGL_PROFILER_FUNCTION();

  const VkDeviceSize requestedAlignedSize = getAlignedSize(size);

  if (shouldAllocateStagingBuffer(requestedAlignedSize, contiguous)) {
    allocateStagingBuffer(nextSize(requestedAlignedSize));
  }

  IGL_DEBUG_ASSERT(!regions_.empty());

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("nextFreeBlock() with %u bytes, aligned %u bytes\n", size, requestedAlignedSize);
#endif

  VkDeviceSize allocatedSize = 0;
  // at this point, there should be a free region that can fit the requested size
  auto regionItr = regions_.begin();
  while (regionItr != regions_.end()) {
    // if requested size is available or if contiguous memory is not requested
    if ((regionItr->size >= requestedAlignedSize || !contiguous) &&
        (immediate_->isReady(regionItr->handle))) {
      allocatedSize = std::min(regionItr->size, requestedAlignedSize);
      break;
    }
    regionItr++;
  }

  IGL_DEBUG_ASSERT(allocatedSize);

  if (allocatedSize) {
    const uint32_t newSize = regionItr->size - allocatedSize;
    const uint32_t newOffset = regionItr->offset + allocatedSize;
    const uint32_t stagingBufferIndex = regionItr->stagingBufferIndex;

    const MemoryRegion allocatedRegion = {regionItr->offset,
                                          allocatedSize,
                                          regionItr->alignedSize,
                                          VulkanImmediateCommands::SubmitHandle(),
                                          stagingBufferIndex};

    // Return this region and add the remaining unused size to the regions_ deque
    IGL_SCOPE_EXIT {
      if (newSize > 0) {
        *regionItr = {newOffset,
                      newSize,
                      regionItr->alignedSize,
                      VulkanImmediateCommands::SubmitHandle(),
                      stagingBufferIndex};
      } else {
        regions_.erase(regionItr);
      }
    };

    freeStagingBufferSize_ -= allocatedSize;
    return allocatedRegion;
  }

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO(
      "Could not find an available block. Waiting for the staging device to become fully "
      "available\n");
#endif

  // Nothing was available. Let's wait for the entire staging buffer to become free
  waitAndReset();

  // try to allocate a new staging buffer
  allocateStagingBuffer(nextSize(requestedAlignedSize));
  IGL_DEBUG_ASSERT(!regions_.empty());

  if (!regions_.empty()) { // if a valid region is available, return it
    freeStagingBufferSize_ -= requestedAlignedSize;
    return regions_.front();
  }
  return {};
}

void VulkanStagingDevice::getBufferSubData(const VulkanBuffer& buffer,
                                           size_t srcOffset,
                                           size_t size,
                                           void* data) {
  IGL_PROFILER_FUNCTION();
  if (buffer.isMapped()) {
    buffer.getBufferSubData(srcOffset, size, data);
    return;
  }

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Download requested for data with %u bytes\n", size);
#endif

  size_t chunkSrcOffset = srcOffset;
  auto* dstData = static_cast<uint8_t*>(data);
  const size_t bufferSize = size;

  while (size) {
    const MemoryRegion memoryChunk = nextFreeBlock(size, false);
    const VkDeviceSize copySize = std::min(static_cast<VkDeviceSize>(size), memoryChunk.size);

    // do the transfer
    const VkBufferCopy copy = {chunkSrcOffset, memoryChunk.offset, copySize};

    const auto& wrapper = immediate_->acquire();

    auto& stagingBuffer = stagingBuffers_[memoryChunk.stagingBufferIndex];

    ctx_.vf_.vkCmdCopyBuffer(
        wrapper.cmdBuf_, buffer.getVkBuffer(), stagingBuffer->getVkBuffer(), 1, &copy);

    // Wait for command to finish
    immediate_->wait(immediate_->submit(wrapper), ctx_.config_.fenceTimeoutNanoseconds);

    // Copy data into data
    const uint8_t* src = stagingBuffer->getMappedPtr() + memoryChunk.offset;
    checked_memcpy(dstData, bufferSize - chunkSrcOffset, src, copySize);

    size -= copySize;
    dstData = (uint8_t*)dstData + copySize;
    chunkSrcOffset += copySize;

    regions_.push_back(memoryChunk);
  }
}

void VulkanStagingDevice::imageData(const VulkanImage& image,
                                    TextureType type,
                                    const TextureRangeDesc& range,
                                    const TextureFormatProperties& properties,
                                    uint32_t bytesPerRow,
                                    const void* data) {
  IGL_PROFILER_FUNCTION();

  const bool is420 = (image.imageFormat_ == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) ||
                     (image.imageFormat_ == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);

  // @fb-only
  const uint32_t storageSize =
      is420 ? image.extent_.width * image.extent_.height * 3u / 2u
            : static_cast<uint32_t>(properties.getBytesPerRange(range, bytesPerRow));

  IGL_DEBUG_ASSERT(storageSize);

  // We don't support uploading image data in small chunks. If the total upload size exceeds the
  // the maximum allowed staging buffer size, we can't upload it
  IGL_DEBUG_ASSERT(storageSize <= maxStagingBufferSize_,
                   "Image size exceeds maximum size of staging buffer");

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Image upload requested for data with %u bytes\n", storageSize);
#endif

  // get next staging buffer free offset
  MemoryRegion memoryChunk = nextFreeBlock(storageSize, true);

  IGL_DEBUG_ASSERT(memoryChunk.size >= storageSize);
  auto& stagingBuffer = stagingBuffers_[memoryChunk.stagingBufferIndex];

  // 1. Copy the pixel data into the host visible staging buffer
  stagingBuffer->bufferSubData(memoryChunk.offset, storageSize, data);

  const auto& wrapper = immediate_->acquire();
  const uint32_t initialLayer = getVkLayer(type, range.face, range.layer);
  const uint32_t numLayers = getVkLayer(type, range.numFaces, range.numLayers);

  std::vector<VkBufferImageCopy> copyRegions;
  copyRegions.reserve(range.numMipLevels);

  // @fb-only
  if (is420) {
    // this is a prototype support implemented for a couple of multiplanar image formats
    IGL_DEBUG_ASSERT(range.face == 0 && range.layer == 0 && range.mipLevel == 0);
    IGL_DEBUG_ASSERT(range.numFaces == 1 && range.numLayers == 1 && range.numMipLevels == 1);
    IGL_DEBUG_ASSERT(range.x == 0 && range.y == 0 && range.z == 0);
    IGL_DEBUG_ASSERT(image.type_ == VK_IMAGE_TYPE_2D);
    IGL_DEBUG_ASSERT(image.extent_.width == range.width && image.extent_.height == range.height);
    const uint32_t w = image.extent_.width;
    const uint32_t h = image.extent_.height;
    ivkCmdBeginDebugUtilsLabel(&ctx_.vf_,
                               wrapper.cmdBuf_,
                               "VulkanStagingDevice::imageData (upload YUV image data)",
                               kColorUploadImage.toFloatPtr());
    VkImageAspectFlags imageAspect = VK_IMAGE_ASPECT_PLANE_0_BIT;

    // Luminance (1 plane)
    copyRegions.emplace_back(
        ivkGetBufferImageCopy2D(memoryChunk.offset,
                                0,
                                ivkGetRect2D(0, 0, w, h),
                                VkImageSubresourceLayers{VK_IMAGE_ASPECT_PLANE_0_BIT, 0, 0, 1}));
    // Chrominance (in 1 or 2 planes, 420 subsampled)
    const VkDeviceSize planeSize0 = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h);
    const VkDeviceSize planeSize1 = planeSize0 / 4; // subsampled
    if (image.imageFormat_ == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
      imageAspect |= VK_IMAGE_ASPECT_PLANE_1_BIT;
      copyRegions.emplace_back(
          ivkGetBufferImageCopy2D(memoryChunk.offset + planeSize0,
                                  0,
                                  ivkGetRect2D(0, 0, w / 2, h / 2),
                                  VkImageSubresourceLayers{VK_IMAGE_ASPECT_PLANE_1_BIT, 0, 0, 1}));
    } else if (image.imageFormat_ == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM) {
      imageAspect |= VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT;
      copyRegions.emplace_back(
          ivkGetBufferImageCopy2D(memoryChunk.offset + planeSize0,
                                  0,
                                  ivkGetRect2D(0, 0, w / 2, h / 2),
                                  VkImageSubresourceLayers{VK_IMAGE_ASPECT_PLANE_1_BIT, 0, 0, 1}));
      copyRegions.emplace_back(
          ivkGetBufferImageCopy2D(memoryChunk.offset + planeSize0 + planeSize1,
                                  0,
                                  ivkGetRect2D(0, 0, w / 2, h / 2),
                                  VkImageSubresourceLayers{VK_IMAGE_ASPECT_PLANE_2_BIT, 0, 0, 1}));

    } else {
      IGL_DEBUG_ABORT("Unimplemented multiplanar image format");
      return;
    }

    const VkImageSubresourceRange subresourceRange = {
        imageAspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
    // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
    ivkImageMemoryBarrier(&ctx_.vf_,
                          wrapper.cmdBuf_,
                          image.getVkImage(),
                          0,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          subresourceRange);

    // 2. Copy the pixel data from the staging buffer into the image
#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdCopyBufferToImage()\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
    ctx_.vf_.vkCmdCopyBufferToImage(wrapper.cmdBuf_,
                                    stagingBuffer->getVkBuffer(),
                                    image.getVkImage(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    static_cast<uint32_t>(copyRegions.size()),
                                    copyRegions.data());

    const VkImageLayout targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 3. Transition TRANSFER_DST_OPTIMAL into `targetLayout`
    ivkImageMemoryBarrier(&ctx_.vf_,
                          wrapper.cmdBuf_,
                          image.getVkImage(),
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          targetLayout,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          subresourceRange);

    image.imageLayout_ = targetLayout;

    ivkCmdEndDebugUtilsLabel(&ctx_.vf_, wrapper.cmdBuf_);

    // Store the allocated block with the SubmitHandle at the end of the deque
    memoryChunk.handle = immediate_->submit(wrapper);
    regions_.push_back(memoryChunk);

    return;

    // end of VK_FORMAT_G8_B8R8_2PLANE_420_UNORM code path
  }

  // vkCmdCopyBufferToImage() can have only one single bit set for image aspect flags (IGL has no
  // way to distinguish between Depth and Stencil for combined depth/stencil image formats)
  const VkImageAspectFlags aspectMask =
      image.isDepthFormat_
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : (image.isStencilFormat_ ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

  ivkCmdBeginDebugUtilsLabel(&ctx_.vf_,
                             wrapper.cmdBuf_,
                             "VulkanStagingDevice::imageData (upload image data)",
                             kColorUploadImage.toFloatPtr());

  for (auto mipLevel = range.mipLevel; mipLevel < range.mipLevel + range.numMipLevels; ++mipLevel) {
    const auto mipRange = range.atMipLevel(mipLevel);
    const uint32_t offset =
        static_cast<uint32_t>(properties.getSubRangeByteOffset(range, mipRange, bytesPerRow));
    const uint32_t texelsPerRow = bytesPerRow / static_cast<uint32_t>(properties.bytesPerBlock);

    if (image.type_ == VK_IMAGE_TYPE_2D) {
      const VkRect2D region = ivkGetRect2D(static_cast<int32_t>(mipRange.x),
                                           static_cast<int32_t>(mipRange.y),
                                           static_cast<uint32_t>(mipRange.width),
                                           static_cast<uint32_t>(mipRange.height));
      copyRegions.emplace_back(ivkGetBufferImageCopy2D(
          memoryChunk.offset + offset,
          texelsPerRow,
          region,
          VkImageSubresourceLayers{
              aspectMask, static_cast<uint32_t>(mipLevel), initialLayer, numLayers}));
    } else {
      copyRegions.emplace_back(ivkGetBufferImageCopy3D(
          memoryChunk.offset + offset,
          texelsPerRow,
          VkOffset3D{static_cast<int32_t>(mipRange.x),
                     static_cast<int32_t>(mipRange.y),
                     static_cast<int32_t>(mipRange.z)},
          VkExtent3D{static_cast<uint32_t>(mipRange.width),
                     static_cast<uint32_t>(mipRange.height),
                     static_cast<uint32_t>(mipRange.depth)},
          VkImageSubresourceLayers{
              aspectMask, static_cast<uint32_t>(mipLevel), initialLayer, numLayers}));
    }
  }

  // image memory barriers should have combined image aspect flags (depth/stencil)
  const VkImageSubresourceRange subresourceRange = {
      image.getImageAspectFlags(),
      static_cast<uint32_t>(0u),
      static_cast<uint32_t>(VK_REMAINING_MIP_LEVELS),
      initialLayer,
      numLayers,
  };
  // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
  ivkImageMemoryBarrier(&ctx_.vf_,
                        wrapper.cmdBuf_,
                        image.getVkImage(),
                        0,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        subresourceRange);

  // 2. Copy the pixel data from the staging buffer into the image
#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdCopyBufferToImage()\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
  ctx_.vf_.vkCmdCopyBufferToImage(wrapper.cmdBuf_,
                                  stagingBuffer->getVkBuffer(),
                                  image.getVkImage(),
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  static_cast<uint32_t>(copyRegions.size()),
                                  copyRegions.data());

  const bool isSampled = (image.getVkImageUsageFlags() & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
  const bool isStorage = (image.getVkImageUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
  const bool isColorAttachment =
      (image.getVkImageUsageFlags() & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0;
  const bool isDepthStencilAttachment =
      (image.getVkImageUsageFlags() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;

  // a ternary cascade...
  const VkImageLayout targetLayout =
      isSampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                : (isStorage ? VK_IMAGE_LAYOUT_GENERAL
                             : (isColorAttachment
                                    ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                    : (isDepthStencilAttachment
                                           ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                           : VK_IMAGE_LAYOUT_UNDEFINED)));

  IGL_DEBUG_ASSERT(targetLayout != VK_IMAGE_LAYOUT_UNDEFINED, "Missing usage flags");

  const VkAccessFlags dstAccessMask =
      isSampled
          ? VK_ACCESS_SHADER_READ_BIT
          : (isStorage ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                       : (isColorAttachment ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                            : (isDepthStencilAttachment
                                                   ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                                   : 0)));

  // 3. Transition TRANSFER_DST_OPTIMAL into `targetLayout`
  ivkImageMemoryBarrier(&ctx_.vf_,
                        wrapper.cmdBuf_,
                        image.getVkImage(),
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        dstAccessMask,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        targetLayout,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        subresourceRange);

  image.imageLayout_ = targetLayout;

  ivkCmdEndDebugUtilsLabel(&ctx_.vf_, wrapper.cmdBuf_);

  // Store the allocated block with the SubmitHandle at the end of the deque
  memoryChunk.handle = immediate_->submit(wrapper);
  regions_.push_back(memoryChunk);
}

void VulkanStagingDevice::getImageData2D(VkImage srcImage,
                                         const uint32_t level,
                                         const uint32_t layer,
                                         const VkRect2D& imageRegion,
                                         TextureFormatProperties properties,
                                         VkFormat /*format*/,
                                         VkImageLayout layout,
                                         void* data,
                                         uint32_t bytesPerRow,
                                         bool flipImageVertical) {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(layout != VK_IMAGE_LAYOUT_UNDEFINED);

  const bool mustRepack = bytesPerRow != 0 && bytesPerRow % properties.bytesPerBlock != 0;

  const auto range =
      TextureRangeDesc::new2D(0, 0, imageRegion.extent.width, imageRegion.extent.height);
  const uint32_t storageSize = static_cast<uint32_t>(
      properties.getBytesPerRange(range.atMipLevel(0), mustRepack ? 0 : bytesPerRow));

  // We don't support uploading image data in small chunks. If the total upload size exceeds the
  // the maximum allowed staging buffer size, we can't upload it
  IGL_DEBUG_ASSERT(storageSize <= maxStagingBufferSize_,
                   "Image size exceeds maximum size of staging buffer");

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Image download requested for data with %u bytes\n", storageSize);
#endif

  // get next staging buffer free offset
  const MemoryRegion memoryChunk = nextFreeBlock(storageSize, true);

  IGL_DEBUG_ASSERT(memoryChunk.size >= storageSize);
  const auto& wrapper1 = immediate_->acquire();

  // 1. Transition to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  ivkImageMemoryBarrier(&ctx_.vf_,
                        wrapper1.cmdBuf_,
                        srcImage,
                        0, // srcAccessMask
                        VK_ACCESS_TRANSFER_READ_BIT, // dstAccessMask
                        layout,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // wait for any previous operation
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
                        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1});

  auto& stagingBuffer = stagingBuffers_[memoryChunk.stagingBufferIndex];

  // 2.  Copy the pixel data from the image into the staging buffer
  const VkBufferImageCopy copy = ivkGetBufferImageCopy2D(
      memoryChunk.offset,
      mustRepack ? 0
                 : bytesPerRow / static_cast<uint32_t>(properties.bytesPerBlock), // bufferRowLength
      imageRegion,
      VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1});
  ctx_.vf_.vkCmdCopyImageToBuffer(wrapper1.cmdBuf_,
                                  srcImage,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  stagingBuffer->getVkBuffer(),
                                  1,
                                  &copy);

  // Wait for command to finish
  immediate_->wait(immediate_->submit(wrapper1), ctx_.config_.fenceTimeoutNanoseconds);

  // 3. Copy data from staging buffer into data
  if (!IGL_DEBUG_VERIFY(stagingBuffer->getMappedPtr())) {
    return;
  }

  const uint8_t* src = stagingBuffer->getMappedPtr() + memoryChunk.offset;
  uint8_t* dst = static_cast<uint8_t*>(data);

  // Vulkan only handles cases where row lengths are multiples of texel block size.
  // Must repack the data if the output data does not conform to this.
  if (mustRepack) {
    // Must repack the data.
    ITexture::repackData(properties, range, src, 0, dst, bytesPerRow, flipImageVertical);
  } else {
    if (flipImageVertical) {
      ITexture::repackData(properties, range, src, bytesPerRow, dst, bytesPerRow, true);
    } else {
      checked_memcpy(dst, storageSize, src, storageSize);
    }
  }

  // 4. Transition back to the initial image layout
  const auto& wrapper2 = immediate_->acquire();

  ivkImageMemoryBarrier(&ctx_.vf_,
                        wrapper2.cmdBuf_,
                        srcImage,
                        VK_ACCESS_TRANSFER_READ_BIT, // srcAccessMask
                        0, // dstAccessMask
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        layout,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // dstStageMask
                        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1});

  // the data should be available as we get out of this function
  immediate_->wait(immediate_->submit(wrapper2), ctx_.config_.fenceTimeoutNanoseconds);

  regions_.push_back(memoryChunk);
  freeStagingBufferSize_ += memoryChunk.size;
}

VkDeviceSize VulkanStagingDevice::getAlignedSize(VkDeviceSize size) const {
  constexpr VkDeviceSize kStagingBufferAlignment = 16; // updated to support BC7 compressed image
  return (size + kStagingBufferAlignment - 1) & ~(kStagingBufferAlignment - 1);
}

void VulkanStagingDevice::waitAndReset() {
  IGL_PROFILER_FUNCTION();

  for (const auto region : regions_) {
    immediate_->wait(region.handle, ctx_.config_.fenceTimeoutNanoseconds);
  }

  regions_.clear();
  stagingBuffers_.clear();
  freeStagingBufferSize_ = 0;
}

bool VulkanStagingDevice::shouldAllocateStagingBuffer(VkDeviceSize sizeNeeded,
                                                      bool contiguous) const noexcept {
  if (regions_.empty()) {
    return true;
  }

  // if contiguous memory is not requested
  if (!contiguous) {
    // return true if there is no free space
    return freeStagingBufferSize_ == 0;
  }

  // if contiguous memory is requested and we have enough free space
  if (sizeNeeded <= freeStagingBufferSize_) {
    // loop over empty regions to find a block that is large enough
    auto regionItr = regions_.begin();
    while (regionItr != regions_.end()) {
      if (regionItr->size >= sizeNeeded && regionItr->handle.empty()) {
        return false;
      }
      regionItr++;
    }
  }

  // return true if no single block can hold the requested size
  return true;
}

VkDeviceSize VulkanStagingDevice::nextSize(VkDeviceSize requestedSize) const {
  return std::min(std::max(getAlignedSize(requestedSize), kMinStagingBufferSize),
                  maxStagingBufferSize_);
}

void VulkanStagingDevice::allocateStagingBuffer(VkDeviceSize minimumSize) {
  IGL_PROFILER_FUNCTION();

  IGL_DEBUG_ASSERT(minimumSize <= maxStagingBufferSize_);

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Allocating a new staging buffer of size %u bytes\n", minimumSize);
#endif

  const auto stagingBufferSize = minimumSize;

  // Increment the id used for naming the staging buffer
  ++stagingBufferCounter_;

  // Create a new staging buffer with the new size
  stagingBuffers_.emplace_back(std::make_unique<VulkanBuffer>(
      ctx_,
      ctx_.device_->getVkDevice(),
      stagingBufferSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      IGL_FORMAT("Buffer: staging buffer #{} with {}B", stagingBufferCounter_, stagingBufferSize)
          .c_str()));
  IGL_DEBUG_ASSERT(stagingBuffers_.back().get());

  // Add region that represents the entire buffer
  regions_.push_front({0,
                       stagingBufferSize,
                       stagingBufferSize,
                       VulkanImmediateCommands::SubmitHandle(),
                       static_cast<uint32_t>(stagingBuffers_.size()) - 1});

  freeStagingBufferSize_ += stagingBufferSize;
}

} // namespace igl::vulkan
