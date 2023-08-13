/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanStagingDevice.h>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

#include <string.h>
#include <algorithm>

#define LVK_VULKAN_DEBUG_STAGING_DEVICE 0

using SubmitHandle = lvk::vulkan::VulkanImmediateCommands::SubmitHandle;

namespace {

/// Vulkan textures are up-side down compared to OGL textures. IGL follows
/// the OGL convention and this function flips the texture vertically
void flipBMP(uint8_t* dstImg, const uint8_t* srcImg, size_t height, size_t bytesPerRow) {
  for (size_t h = 0; h < height; h++) {
    memcpy(dstImg + h * bytesPerRow, srcImg + bytesPerRow * (height - 1 - h), bytesPerRow);
  }
}

} // namespace

namespace lvk {

namespace vulkan {

VulkanStagingDevice::VulkanStagingDevice(VulkanContext& ctx) : ctx_(ctx) {
  LVK_PROFILER_FUNCTION();

  const auto& limits = ctx_.getVkPhysicalDeviceProperties().limits;

  // Use default value of 256 MB, and clamp it to the max limits
  stagingBufferSize_ = std::min(limits.maxStorageBufferRange, 256u * 1024u * 1024u);

  bufferCapacity_ = stagingBufferSize_;

  stagingBuffer_ = ctx_.createBuffer(stagingBufferSize_,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                     nullptr,
                                     "Buffer: staging buffer");
  LVK_ASSERT(!stagingBuffer_.empty());

  immediate_ = std::make_unique<lvk::vulkan::VulkanImmediateCommands>(
      ctx_.getVkDevice(), ctx_.deviceQueues_.graphicsQueueFamilyIndex, "VulkanStagingDevice::immediate_");
}

VulkanStagingDevice::~VulkanStagingDevice() {
  ctx_.buffersPool_.destroy(stagingBuffer_);
}

void VulkanStagingDevice::bufferSubData(VulkanBuffer& buffer, size_t dstOffset, size_t size, const void* data) {
  LVK_PROFILER_FUNCTION();
  if (buffer.isMapped()) {
    buffer.bufferSubData(dstOffset, size, data);
    return;
  }

  size_t chunkDstOffset = dstOffset;
  void* copyData = const_cast<void*>(data);

  lvk::vulkan::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  while (size) {
    // get next staging buffer free offset
    const MemoryRegionDesc desc = getNextFreeOffset((uint32_t)size);
    const uint32_t chunkSize = std::min((uint32_t)size, desc.alignedSize_);

    // copy data into staging buffer
    stagingBuffer->bufferSubData(desc.srcOffset_, chunkSize, copyData);

    // do the transfer
    const VkBufferCopy copy = {desc.srcOffset_, chunkDstOffset, chunkSize};

    auto& wrapper = immediate_->acquire();
    vkCmdCopyBuffer(wrapper.cmdBuf_, stagingBuffer->getVkBuffer(), buffer.getVkBuffer(), 1, &copy);
    const SubmitHandle fenceId = immediate_->submit(wrapper);
    outstandingFences_[fenceId.handle()] = desc;

    size -= chunkSize;
    copyData = (uint8_t*)copyData + chunkSize;
    chunkDstOffset += chunkSize;
  }
}

void VulkanStagingDevice::getBufferSubData(VulkanBuffer& buffer, size_t srcOffset, size_t size, void* data) {
  LVK_PROFILER_FUNCTION();
  if (buffer.isMapped()) {
    buffer.getBufferSubData(srcOffset, size, data);
    return;
  }

  size_t chunkSrcOffset = srcOffset;
  auto* dstData = static_cast<uint8_t*>(data);

  lvk::vulkan::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  while (size) {
    // get next staging buffer free offset
    const MemoryRegionDesc desc = getNextFreeOffset((uint32_t)size);
    const uint32_t chunkSize = std::min((uint32_t)size, desc.alignedSize_);

    // do the transfer
    const VkBufferCopy copy = {chunkSrcOffset, desc.srcOffset_, chunkSize};

    auto& wrapper = immediate_->acquire();

    vkCmdCopyBuffer(wrapper.cmdBuf_, buffer.getVkBuffer(), stagingBuffer->getVkBuffer(), 1, &copy);

    SubmitHandle fenceId = immediate_->submit(wrapper);
    outstandingFences_[fenceId.handle()] = desc;

    // Wait for command to finish
    flushOutstandingFences();

    // copy data into data
    const uint8_t* src = stagingBuffer->getMappedPtr() + desc.srcOffset_;
    memcpy(dstData, src, chunkSize);

    size -= chunkSize;
    dstData = (uint8_t*)dstData + chunkSize;
    chunkSrcOffset += chunkSize;
  }
}

void VulkanStagingDevice::imageData2D(VulkanImage& image,
                                      const VkRect2D& imageRegion,
                                      uint32_t baseMipLevel,
                                      uint32_t numMipLevels,
                                      uint32_t layer,
                                      uint32_t numLayers,
                                      VkFormat format,
                                      const void* data[]) {
  LVK_PROFILER_FUNCTION();

  // cache the dimensions of each mip-level for later
  uint32_t mipSizes[32];

  LVK_ASSERT(numMipLevels <= 32);

  // divide the width and height by 2 until we get to the size of level 'baseMipLevel'
  auto width = image.extent_.width >> baseMipLevel;
  auto height = image.extent_.height >> baseMipLevel;

  const Format texFormat(vkFormatToFormat(format));

  LVK_ASSERT_MSG(imageRegion.offset.x == 0 && imageRegion.offset.y == 0 && imageRegion.extent.width == width &&
                     imageRegion.extent.height == height,
                 "Uploading mip levels with an image region that is smaller than the base mip level is "
                 "not supported");

  // find the storage size for all mip-levels being uploaded
  uint32_t layarStorageSize = 0;
  for (uint32_t i = 0; i < numMipLevels; ++i) {
    const uint32_t mipSize = lvk::getTextureBytesPerLayer(image.extent_.width, image.extent_.height, texFormat, i);
    layarStorageSize += mipSize;
    mipSizes[i] = mipSize;
    width = width <= 1 ? 1 : width >> 1;
    height = height <= 1 ? 1 : height >> 1;
  }
  const uint32_t storageSize = layarStorageSize * numLayers;

  LVK_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(layarStorageSize);
  // currently, no support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storageSize, we will wait for GPU idle and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }
  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  auto& wrapper = immediate_->acquire();

  lvk::vulkan::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  for (uint32_t layer = 0; layer != numLayers; layer++) {
    // copy the pixel data into the host visible staging buffer
    stagingBuffer->bufferSubData(desc.srcOffset_ + layer * layarStorageSize, layarStorageSize, data[layer]);

    uint32_t mipLevelOffset = 0;

    for (uint32_t mipLevel = 0; mipLevel < numMipLevels; ++mipLevel) {
      const auto currentMipLevel = baseMipLevel + mipLevel;

      LVK_ASSERT(currentMipLevel < image.levels_);
      LVK_ASSERT(mipLevel < image.levels_);

      // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
      lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                              image.getVkImage(),
                              0,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, 1, layer, 1});

#if LVK_VULKAN_PRINT_COMMANDS
      LLOGL("%p vkCmdCopyBufferToImage()\n", wrapper.cmdBuf_);
#endif // LVK_VULKAN_PRINT_COMMANDS
      // 2. Copy the pixel data from the staging buffer into the image
      const VkRect2D region = {
          .offset = {.x = imageRegion.offset.x >> mipLevel, .y = imageRegion.offset.y >> mipLevel},
          .extent = {.width = std::max(1u, imageRegion.extent.width >> mipLevel),
                     .height = std::max(1u, imageRegion.extent.height >> mipLevel)},
      };
      const VkBufferImageCopy copy = {
          // the offset for this level is at the start of all mip-levels plus the size of all previous mip-levels being uploaded
          .bufferOffset = desc.srcOffset_ + layer * layarStorageSize + mipLevelOffset,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, layer, 1},
          .imageOffset = {.x = region.offset.x, .y = region.offset.y, .z = 0},
          .imageExtent = {.width = region.extent.width, .height = region.extent.height, .depth = 1u},
      };
      vkCmdCopyBufferToImage(
          wrapper.cmdBuf_, stagingBuffer->getVkBuffer(), image.getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
      lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                              image.getVkImage(),
                              VK_ACCESS_TRANSFER_READ_BIT, // VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                              VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, 1, layer, 1});

      // Compute the offset for the next level
      mipLevelOffset += mipSizes[mipLevel];
    }
  }

  image.imageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  SubmitHandle fenceId = immediate_->submit(wrapper);
  outstandingFences_[fenceId.handle()] = desc;
}

void VulkanStagingDevice::imageData3D(VulkanImage& image,
                                      const VkOffset3D& offset,
                                      const VkExtent3D& extent,
                                      VkFormat format,
                                      const void* data) {
  LVK_PROFILER_FUNCTION();
  LVK_ASSERT_MSG(image.levels_ == 1, "Can handle only 3D images with exactly 1 mip-level");
  LVK_ASSERT_MSG((offset.x == 0) && (offset.y == 0) && (offset.z == 0), "Can upload only full-size 3D images");
  const uint32_t storageSize = extent.width * extent.height * extent.depth * getBytesPerPixel(format);

  LVK_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(storageSize);

  // currently, no support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storageSize, we will wait for gpu idle and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }

  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  lvk::vulkan::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  // 1. Copy the pixel data into the host visible staging buffer
  stagingBuffer->bufferSubData(desc.srcOffset_, storageSize, data);

  auto& wrapper = immediate_->acquire();

  // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
  lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                          image.getVkImage(),
                          0,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // 2. Copy the pixel data from the staging buffer into the image
  const VkBufferImageCopy copy = {
      .bufferOffset = desc.srcOffset_,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageOffset = offset,
      .imageExtent = extent,
  };
  vkCmdCopyBufferToImage(wrapper.cmdBuf_, stagingBuffer->getVkBuffer(), image.getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
  lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                          image.getVkImage(),
                          VK_ACCESS_TRANSFER_READ_BIT, // VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  image.imageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  SubmitHandle fenceId = immediate_->submit(wrapper);
  outstandingFences_[fenceId.handle()] = desc;
}

void VulkanStagingDevice::getImageData2D(VkImage srcImage,
                                         const uint32_t level,
                                         const uint32_t layer,
                                         const VkRect2D& imageRegion,
                                         VkFormat format,
                                         VkImageLayout layout,
                                         void* data,
                                         uint32_t dataBytesPerRow,
                                         bool flipImageVertical) {
  LVK_PROFILER_FUNCTION();
  LVK_ASSERT(layout != VK_IMAGE_LAYOUT_UNDEFINED);

  const uint32_t storageSize = imageRegion.extent.width * imageRegion.extent.height * getBytesPerPixel(format);
  LVK_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(storageSize);

  // If we get smaller buffer size than storageSize, we will wait for gpu idle
  // and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }

  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  auto& wrapper1 = immediate_->acquire();

  lvk::vulkan::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  // 1. Transition to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  lvk::imageMemoryBarrier(wrapper1.cmdBuf_,
                          srcImage,
                          0, // srcAccessMask
                          VK_ACCESS_TRANSFER_READ_BIT, // dstAccessMask
                          layout,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // wait for any previous operation
                          VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1});

  // 2.  Copy the pixel data from the image into the staging buffer
  const VkBufferImageCopy copy = {
      .bufferOffset = desc.srcOffset_,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1},
      .imageOffset = {.x = imageRegion.offset.x, .y = imageRegion.offset.y, .z = 0},
      .imageExtent = {.width = imageRegion.extent.width, .height = imageRegion.extent.height, .depth = 1u},
  };
  vkCmdCopyImageToBuffer(wrapper1.cmdBuf_, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer->getVkBuffer(), 1, &copy);

  SubmitHandle fenceId = immediate_->submit(wrapper1);
  outstandingFences_[fenceId.handle()] = desc;

  flushOutstandingFences();

  // 3. Copy data from staging buffer into data
  if (!LVK_VERIFY(stagingBuffer->getMappedPtr())) {
    return;
  }

  const uint8_t* src = stagingBuffer->getMappedPtr() + desc.srcOffset_;
  uint8_t* dst = static_cast<uint8_t*>(data);

  if (flipImageVertical) {
    flipBMP(dst, src, imageRegion.extent.height, imageRegion.extent.width * getBytesPerPixel(format));
  } else {
    memcpy(dst, src, storageSize);
  }

  // 4. Transition back to the initial image layout
  auto& wrapper2 = immediate_->acquire();

  lvk::imageMemoryBarrier(wrapper2.cmdBuf_,
                          srcImage,
                          VK_ACCESS_TRANSFER_READ_BIT, // srcAccessMask
                          0, // dstAccessMask
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          layout,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // dstStageMask
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1});

  fenceId = immediate_->submit(wrapper2);
  outstandingFences_[fenceId.handle()] = desc;
}

uint32_t VulkanStagingDevice::getAlignedSize(uint32_t size) const {
  return (size + stagingBufferAlignment_ - 1) & ~(stagingBufferAlignment_ - 1);
}

VulkanStagingDevice::MemoryRegionDesc VulkanStagingDevice::getNextFreeOffset(uint32_t size) {
  LVK_PROFILER_FUNCTION();
  uint32_t alignedSize = getAlignedSize(size);

  // track maximum previously used region
  MemoryRegionDesc maxRegionDesc;
  SubmitHandle maxRegionFence = SubmitHandle();

  // check if we can reuse any of previously used memory region
  for (auto [handle, desc] : outstandingFences_) {
    if (immediate_->isReady(SubmitHandle(handle))) {
      if (desc.alignedSize_ >= alignedSize) {
        outstandingFences_.erase(handle);
#if LVK_VULKAN_DEBUG_STAGING_DEVICE
        LLOGL("Reusing memory region %u bytes\n", desc.alignedSize_);
#endif
        return desc;
      }

      if (maxRegionDesc.alignedSize_ < desc.alignedSize_) {
        maxRegionDesc = desc;
        maxRegionFence = SubmitHandle(handle);
      }
    }
  }

  if (!maxRegionFence.empty() && bufferCapacity_ < maxRegionDesc.alignedSize_) {
    outstandingFences_.erase(maxRegionFence.handle());
#if LVK_VULKAN_DEBUG_STAGING_DEVICE
    LLOGL("Reusing memory region %u bytes\n", maxRegionDesc.alignedSize_);
#endif
    return maxRegionDesc;
  }

  if (bufferCapacity_ == 0) {
    // no more space available in the staging buffer
    flushOutstandingFences();
  }

  // allocate from free staging memory
  alignedSize = (alignedSize <= bufferCapacity_) ? alignedSize : bufferCapacity_;
  const uint32_t srcOffset = stagingBufferFrontOffset_;
  stagingBufferFrontOffset_ = (stagingBufferFrontOffset_ + alignedSize) % stagingBufferSize_;
  bufferCapacity_ -= alignedSize;

#if LVK_VULKAN_DEBUG_STAGING_DEVICE
  LLOGL("Allocating new memory region: %u bytes\n", alignedSize);
#endif

  return {srcOffset, alignedSize};
}

void VulkanStagingDevice::flushOutstandingFences() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_WAIT);

#if LVK_VULKAN_DEBUG_STAGING_DEVICE
  LLOGL("StagingDevice - Wait for Idle\n");
#endif
  std::for_each(outstandingFences_.begin(), outstandingFences_.end(), [this](std::pair<uint64_t, MemoryRegionDesc> const& pair) {
    immediate_->wait(SubmitHandle(pair.first));
  });

  outstandingFences_.clear();
  stagingBufferFrontOffset_ = 0;
  bufferCapacity_ = stagingBufferSize_;
}

} // namespace vulkan
} // namespace lvk
