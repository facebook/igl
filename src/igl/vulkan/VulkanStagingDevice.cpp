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

namespace igl {

namespace vulkan {

VulkanStagingDevice::VulkanStagingDevice(VulkanContext& ctx) : ctx_(ctx) {
  IGL_PROFILER_FUNCTION();

  const auto& limits = ctx_.getVkPhysicalDeviceProperties().limits;

  // Use value of 256MB (limited by some architectures), and clamp it to the max limits
  maxBufferCapacity_ = std::min(limits.maxStorageBufferRange, 256u * 1024u * 1024u);

  // Use default value maxBufferCapacity_
  stagingBufferSize_ = maxBufferCapacity_;

  // Initialize the block list with a block that spans the entire staging buffer
  regions_.push_front(MemoryRegion{0, stagingBufferSize_, VulkanImmediateCommands::SubmitHandle()});

  stagingBuffer_ =
      ctx_.createBuffer(stagingBufferSize_,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                        nullptr,
                        "Buffer: staging buffer");
  IGL_ASSERT(stagingBuffer_.get());

  immediate_ = std::make_unique<igl::vulkan::VulkanImmediateCommands>(
      ctx_.device_->getVkDevice(),
      ctx_.deviceQueues_.graphicsQueueFamilyIndex,
      "VulkanStagingDevice::immediate_");
  IGL_ASSERT(immediate_.get());
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

  while (size) {
    // finds a free memory block to store the data in the staging buffer
    MemoryRegion memoryChunk = nextFreeBlock(size);
    const auto copySize = std::min(static_cast<uint32_t>(size), memoryChunk.size);

    // copy data into the staging buffer
    stagingBuffer_->bufferSubData(memoryChunk.offset, copySize, copyData);

    // do the transfer
    const VkBufferCopy copy = {memoryChunk.offset, chunkDstOffset, copySize};

    auto& wrapper = immediate_->acquire();
    vkCmdCopyBuffer(wrapper.cmdBuf_, stagingBuffer_->getVkBuffer(), buffer.getVkBuffer(), 1, &copy);
    memoryChunk.handle = immediate_->submit(wrapper); // store the submit handle with the allocation
    regions_.push_back(memoryChunk);

    size -= copySize;
    copyData = (uint8_t*)copyData + copySize;
    chunkDstOffset += copySize;
  }
}

VulkanStagingDevice::MemoryRegion VulkanStagingDevice::nextFreeBlock(uint32_t size) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(!regions_.empty());

  const auto requestedAlignedSize = getAlignedSize(size);

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO(
      "Requesting new memory region with %u bytes, aligned %u bytes\n", size, requestedAlignedSize);
#endif

  // If we can't find an empty free region that is as big as the requestedAlignedSize, return
  // whatever we could find, which will be stored in bestNextIt
  auto bestNextIt = regions_.begin();

  for (auto it = regions_.begin(); it != regions_.end(); ++it) {
    if (immediate_->isReady(it->handle)) {
      // This region is free, but is it big enough?
      if (it->size > requestedAlignedSize) {
        // It is big enough!
        const uint32_t unusedSize = it->size - requestedAlignedSize;
        const uint32_t unusedOffset = it->offset + requestedAlignedSize;

        // Return this region and add the remaining unused size to the regions_ deque
        IGL_SCOPE_EXIT {
          regions_.erase(it);
          if (unusedSize > 0) {
            regions_.push_front(
                {unusedOffset, unusedSize, VulkanImmediateCommands::SubmitHandle()});
          }
        };

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
        IGL_LOG_INFO("Found block with %u bytes at offset %u\n", it->size, it->offset);
#endif

        return {it->offset, requestedAlignedSize, VulkanImmediateCommands::SubmitHandle()};
      }

      // Cache the largest available region that isn't as big as the one we're looking for
      if (it->size > bestNextIt->size) {
        bestNextIt = it;
      }
    }
  }

  // We found a region that is available that is smaller than the requested size. It's the best we
  // can do
  if (bestNextIt != regions_.end() && immediate_->isReady(bestNextIt->handle)) {
    IGL_SCOPE_EXIT {
      regions_.erase(bestNextIt);
    };

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
    IGL_LOG_INFO("Found block smaller than size needed with %u bytes at offset %u\n",
                 bestNextIt->size,
                 bestNextIt->offset);
#endif

    return {bestNextIt->offset, bestNextIt->size, VulkanImmediateCommands::SubmitHandle()};
  }

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO(
      "Didn't find available block. Waiting for staging device to become fully available\n");
#endif

  // Nothing was available. Let's wait for the entire staging buffer to become free
  waitAndReset();

  // waitAndReset() adds a region that spans the entire buffer. Since we'll be using part of it, we
  // need to replace it with a used block and an unused portion
  regions_.clear();

  // Store the unused size in the deque first...
  const uint32_t unusedSize = stagingBufferSize_ - requestedAlignedSize;
  if (unusedSize > 0) {
    const uint32_t unusedOffset = requestedAlignedSize;
    regions_.push_front({unusedOffset, unusedSize, VulkanImmediateCommands::SubmitHandle()});
  }

  //... and then return the smallest free region that can hold the requested size
  return {0, requestedAlignedSize, VulkanImmediateCommands::SubmitHandle()};
}

void VulkanStagingDevice::getBufferSubData(VulkanBuffer& buffer,
                                           size_t srcOffset,
                                           size_t size,
                                           void* data) {
  IGL_PROFILER_FUNCTION();
  if (buffer.isMapped()) {
    buffer.getBufferSubData(srcOffset, size, data);
    return;
  }

  size_t chunkSrcOffset = srcOffset;
  auto* dstData = static_cast<uint8_t*>(data);

  while (size) {
    MemoryRegion memoryChunk = nextFreeBlock(size);
    const uint32_t copySize = std::min(static_cast<uint32_t>(size), memoryChunk.size);

    // do the transfer
    const VkBufferCopy copy = {chunkSrcOffset, memoryChunk.offset, copySize};

    auto& wrapper = immediate_->acquire();

    vkCmdCopyBuffer(wrapper.cmdBuf_, buffer.getVkBuffer(), stagingBuffer_->getVkBuffer(), 1, &copy);

    // Wait for command to finish
    immediate_->wait(immediate_->submit(wrapper));

    // Copy data into data
    const uint8_t* src = stagingBuffer_->getMappedPtr() + memoryChunk.offset;
    checked_memcpy(dstData, size - chunkSrcOffset, src, memoryChunk.size);

    size -= copySize;
    dstData = (uint8_t*)dstData + copySize;
    chunkSrcOffset += copySize;

    regions_.push_back(memoryChunk);
  }
}

void VulkanStagingDevice::imageData(VulkanImage& image,
                                    TextureType type,
                                    const TextureRangeDesc& range,
                                    const TextureFormatProperties& properties,
                                    uint32_t bytesPerRow,
                                    const void* data) {
  IGL_PROFILER_FUNCTION();

  const uint32_t storageSize =
      static_cast<uint32_t>(properties.getBytesPerRange(range, bytesPerRow));

  IGL_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegion memoryChunk = nextFreeBlock(storageSize);

  // currently, no support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storageSize, we will wait for gpu idle and get bigger chunk.
  if (memoryChunk.size < storageSize) {
    waitAndReset();
    memoryChunk = nextFreeBlock(storageSize);
  }

  IGL_ASSERT(memoryChunk.size >= storageSize);

  // 1. Copy the pixel data into the host visible staging buffer
  stagingBuffer_->bufferSubData(memoryChunk.offset, storageSize, data);

  auto& wrapper = immediate_->acquire();
  const uint32_t initialLayer = getVkLayer(type, range.face, range.layer);
  const uint32_t numLayers = getVkLayer(type, range.numFaces, range.numLayers);

  std::vector<VkBufferImageCopy> copyRegions;
  copyRegions.reserve(range.numMipLevels);

  // vkCmdCopyBufferToImage() can have only one single bit set for image aspect flags (IGL has no
  // way to distinguish between Depth and Stencil for combined depth/stencil image formats)
  const VkImageAspectFlags aspectMask =
      image.isDepthFormat_
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : (image.isStencilFormat_ ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

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
      static_cast<uint32_t>(range.mipLevel),
      static_cast<uint32_t>(range.numMipLevels),
      initialLayer,
      numLayers,
  };
  // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
  ivkImageMemoryBarrier(wrapper.cmdBuf_,
                        image.getVkImage(),
                        0,
                        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        subresourceRange);

  // 2. Copy the pixel data from the staging buffer into the image
#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdCopyBufferToImage()\n", wrapper.cmdBuf_);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdCopyBufferToImage(wrapper.cmdBuf_,
                         stagingBuffer_->getVkBuffer(),
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

  IGL_ASSERT_MSG(targetLayout != VK_IMAGE_LAYOUT_UNDEFINED, "Missing usage flags");

  const VkAccessFlags dstAccessMask =
      isSampled
          ? VK_ACCESS_SHADER_READ_BIT
          : (isStorage ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                       : (isColorAttachment ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                            : (isDepthStencilAttachment
                                                   ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                                   : 0)));

  // 3. Transition TRANSFER_DST_OPTIMAL into `targetLayout`
  ivkImageMemoryBarrier(wrapper.cmdBuf_,
                        image.getVkImage(),
                        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                        dstAccessMask,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        targetLayout,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        subresourceRange);

  image.imageLayout_ = targetLayout;

  // Store the allocated block with the SubmitHandle at the end of the deque
  memoryChunk.handle = immediate_->submit(wrapper);
  regions_.push_back(memoryChunk);
}

void VulkanStagingDevice::getImageData2D(VkImage srcImage,
                                         const uint32_t level,
                                         const uint32_t layer,
                                         const VkRect2D& imageRegion,
                                         TextureFormatProperties properties,
                                         VkFormat format,
                                         VkImageLayout layout,
                                         void* data,
                                         uint32_t bytesPerRow,
                                         bool flipImageVertical) {
  IGL_PROFILER_FUNCTION();
  IGL_ASSERT(layout != VK_IMAGE_LAYOUT_UNDEFINED);

  bool mustRepack = bytesPerRow != 0 && bytesPerRow % properties.bytesPerBlock != 0;

  const auto range =
      TextureRangeDesc::new2D(0, 0, imageRegion.extent.width, imageRegion.extent.height);
  const uint32_t storageSize = static_cast<uint32_t>(
      properties.getBytesPerRange(range.atMipLevel(0), mustRepack ? 0 : bytesPerRow));
  IGL_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegion memoryChunk = nextFreeBlock(storageSize);

  // If we get smaller buffer size than storageSize, we will wait for gpu idle
  // and get bigger chunk.
  if (memoryChunk.size < storageSize) {
    waitAndReset();
    memoryChunk = nextFreeBlock(storageSize);
  }

  IGL_ASSERT(memoryChunk.size >= storageSize);
  auto& wrapper1 = immediate_->acquire();

  // 1. Transition to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  ivkImageMemoryBarrier(wrapper1.cmdBuf_,
                        srcImage,
                        0, // srcAccessMask
                        VK_ACCESS_TRANSFER_READ_BIT, // dstAccessMask
                        layout,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // wait for any previous operation
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
                        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1});

  // 2.  Copy the pixel data from the image into the staging buffer
  const VkBufferImageCopy copy = ivkGetBufferImageCopy2D(
      memoryChunk.offset,
      mustRepack ? 0
                 : bytesPerRow / static_cast<uint32_t>(properties.bytesPerBlock), // bufferRowLength
      imageRegion,
      VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1});
  vkCmdCopyImageToBuffer(wrapper1.cmdBuf_,
                         srcImage,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         stagingBuffer_->getVkBuffer(),
                         1,
                         &copy);

  // Wait for command to finish
  immediate_->wait(immediate_->submit(wrapper1));

  // 3. Copy data from staging buffer into data
  if (!IGL_VERIFY(stagingBuffer_->getMappedPtr())) {
    return;
  }

  const uint8_t* src = stagingBuffer_->getMappedPtr() + memoryChunk.offset;
  uint8_t* dst = static_cast<uint8_t*>(data);

  // Vulkan only handles cases where row lengths are multiples of texel block size.
  // Must repack the data if the output data does not conform to this.
  if (mustRepack) {
    // Must repack the data.
    ITexture::repackData(properties, range, src, 0, dst, bytesPerRow, flipImageVertical);
  } else {
    if (flipImageVertical) {
      ITexture::repackData(properties, range, src, 0, dst, 0, true);
    } else {
      checked_memcpy(dst, storageSize, src, storageSize);
    }
  }

  // 4. Transition back to the initial image layout
  auto& wrapper2 = immediate_->acquire();

  ivkImageMemoryBarrier(wrapper2.cmdBuf_,
                        srcImage,
                        VK_ACCESS_TRANSFER_READ_BIT, // srcAccessMask
                        0, // dstAccessMask
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        layout,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // dstStageMask
                        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1});

  // the data should be available as we get out of this function
  immediate_->wait(immediate_->submit(wrapper2));
  regions_.push_back(memoryChunk);
}

uint32_t VulkanStagingDevice::getAlignedSize(uint32_t size) const {
  constexpr uint32_t kStagingBufferAlignment = 16; // updated to support BC7 compressed image
  return (size + kStagingBufferAlignment - 1) & ~(kStagingBufferAlignment - 1);
}

void VulkanStagingDevice::waitAndReset() {
  IGL_PROFILER_FUNCTION();

  for (const auto region : regions_) {
    immediate_->wait(region.handle);
  }

  regions_.clear();

  regions_.push_front({0, stagingBufferSize_, VulkanImmediateCommands::SubmitHandle()});
}

bool VulkanStagingDevice::shouldGrowStagingBuffer(uint32_t sizeNeeded) const {
  return !stagingBuffer_ || (sizeNeeded > stagingBufferSize_);
}

uint32_t VulkanStagingDevice::nextSize(uint32_t requestedSize) const {
  return std::min(getAlignedSize(requestedSize), maxBufferCapacity_);
}

void VulkanStagingDevice::growStagingBuffer(uint32_t minimumSize) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(minimumSize <= maxBufferCapacity_);

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Growing staging buffer from %u to %u bytes\n", stagingBufferSize_, minimumSize);
#endif

  waitAndReset();

  // De-allocates the staging buffer
  stagingBuffer_ = nullptr;

  // If the size of the new staging buffer plus the size of the existing one is larger than the
  // limit imposed by some architectures on buffers that are device and host visible, we need to
  // wait for the current buffer to be destroyed before we can allocate a new one
  if ((minimumSize + stagingBufferSize_) > maxBufferCapacity_) {
    // Wait for the current buffer to be destroyed on the device
    ctx_.waitDeferredTasks();
  }

  stagingBufferSize_ = minimumSize;

  // Create a new staging buffer with the new size
  stagingBuffer_ =
      ctx_.createBuffer(stagingBufferSize_,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                        nullptr,
                        "Buffer: staging buffer");
  IGL_ASSERT(stagingBuffer_.get());

  // Clear out the old regions and add one that represents the entire buffer
  regions_.clear();
  regions_.push_front({0, stagingBufferSize_, VulkanImmediateCommands::SubmitHandle()});
}

} // namespace vulkan
} // namespace igl
