/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanStagingDevice.h>

#include <igl/vulkan/VulkanContext.h>
#include <lvk/vulkan/VulkanClasses.h>
#include <lvk/vulkan/VulkanUtils.h>

#include <string.h>
#include <algorithm>

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

  immediate_ = std::make_unique<lvk::VulkanImmediateCommands>(
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

  lvk::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  while (size) {
    // get next staging buffer free offset
    const MemoryRegionDesc desc = getNextFreeOffset((uint32_t)size);
    const uint32_t chunkSize = std::min((uint32_t)size, desc.alignedSize_);

    // copy data into staging buffer
    stagingBuffer->bufferSubData(desc.srcOffset_, chunkSize, data);

    // do the transfer
    const VkBufferCopy copy = {desc.srcOffset_, dstOffset, chunkSize};

    auto& wrapper = immediate_->acquire();
    vkCmdCopyBuffer(wrapper.cmdBuf_, stagingBuffer->vkBuffer_, buffer.vkBuffer_, 1, &copy);
    const SubmitHandle fenceId = immediate_->submit(wrapper);
    outstandingFences_[fenceId.handle()] = desc;

    size -= chunkSize;
    data = (uint8_t*)data + chunkSize;
    dstOffset += chunkSize;
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
  uint32_t mipSizes[LVK_MAX_MIP_LEVELS];

  LVK_ASSERT(numMipLevels <= LVK_MAX_MIP_LEVELS);

  // divide the width and height by 2 until we get to the size of level 'baseMipLevel'
  uint32_t width = image.vkExtent_.width >> baseMipLevel;
  uint32_t height = image.vkExtent_.height >> baseMipLevel;

  const Format texFormat(vkFormatToFormat(format));

  LVK_ASSERT_MSG(!imageRegion.offset.x && !imageRegion.offset.y && imageRegion.extent.width == width && imageRegion.extent.height == height,
                 "Uploading mip-levels with an image region that is smaller than the base mip level is not supported");

  // find the storage size for all mip-levels being uploaded
  uint32_t layarStorageSize = 0;
  for (uint32_t i = 0; i < numMipLevels; ++i) {
    const uint32_t mipSize = lvk::getTextureBytesPerLayer(image.vkExtent_.width, image.vkExtent_.height, texFormat, i);
    layarStorageSize += mipSize;
    mipSizes[i] = mipSize;
    width = width <= 1 ? 1 : width >> 1;
    height = height <= 1 ? 1 : height >> 1;
  }
  const uint32_t storageSize = layarStorageSize * numLayers;

  LVK_ASSERT(storageSize <= stagingBufferSize_);

  MemoryRegionDesc desc = getNextFreeOffset(layarStorageSize);
  // No support for copying image in multiple smaller chunk sizes. If we get smaller buffer size than storageSize, we will wait for GPU idle
  // and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }
  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  auto& wrapper = immediate_->acquire();

  lvk::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  for (uint32_t layer = 0; layer != numLayers; layer++) {
    stagingBuffer->bufferSubData(desc.srcOffset_ + layer * layarStorageSize, layarStorageSize, data[layer]);

    uint32_t mipLevelOffset = 0;

    for (uint32_t mipLevel = 0; mipLevel < numMipLevels; ++mipLevel) {
      const auto currentMipLevel = baseMipLevel + mipLevel;

      LVK_ASSERT(currentMipLevel < image.numLevels_);
      LVK_ASSERT(mipLevel < image.numLevels_);

      // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
      lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                              image.vkImage_,
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
      vkCmdCopyBufferToImage(wrapper.cmdBuf_, stagingBuffer->vkBuffer_, image.vkImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
      lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                              image.vkImage_,
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

  image.vkImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  SubmitHandle fenceId = immediate_->submit(wrapper);
  outstandingFences_[fenceId.handle()] = desc;
}

void VulkanStagingDevice::imageData3D(VulkanImage& image,
                                      const VkOffset3D& offset,
                                      const VkExtent3D& extent,
                                      VkFormat format,
                                      const void* data) {
  LVK_PROFILER_FUNCTION();
  LVK_ASSERT_MSG(image.numLevels_ == 1, "Can handle only 3D images with exactly 1 mip-level");
  LVK_ASSERT_MSG((offset.x == 0) && (offset.y == 0) && (offset.z == 0), "Can upload only full-size 3D images");
  const uint32_t storageSize = extent.width * extent.height * extent.depth * getBytesPerPixel(format);

  LVK_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(storageSize);

  // No support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storageSize, we will wait for GPU idle and get a bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }

  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  lvk::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  // 1. Copy the pixel data into the host visible staging buffer
  stagingBuffer->bufferSubData(desc.srcOffset_, storageSize, data);

  auto& wrapper = immediate_->acquire();

  // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
  lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                          image.vkImage_,
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
  vkCmdCopyBufferToImage(wrapper.cmdBuf_, stagingBuffer->vkBuffer_, image.vkImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
  lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                          image.vkImage_,
                          VK_ACCESS_TRANSFER_READ_BIT, // VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  image.vkImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  SubmitHandle fenceId = immediate_->submit(wrapper);
  outstandingFences_[fenceId.handle()] = desc;
}

VulkanStagingDevice::MemoryRegionDesc VulkanStagingDevice::getNextFreeOffset(uint32_t size) {
  LVK_PROFILER_FUNCTION();

  uint32_t alignedSize = (size + stagingBufferAlignment_ - 1) & ~(stagingBufferAlignment_ - 1);

  // track maximum previously used region
  MemoryRegionDesc maxRegionDesc;
  SubmitHandle maxRegionFence = SubmitHandle();

  // check if we can reuse any of previously used memory region
  for (auto [handle, desc] : outstandingFences_) {
    if (immediate_->isReady(SubmitHandle(handle))) {
      if (desc.alignedSize_ >= alignedSize) {
        outstandingFences_.erase(handle);
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

  return {srcOffset, alignedSize};
}

void VulkanStagingDevice::flushOutstandingFences() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_WAIT);

  std::for_each(outstandingFences_.begin(), outstandingFences_.end(), [this](std::pair<uint64_t, MemoryRegionDesc> const& pair) {
    immediate_->wait(SubmitHandle(pair.first));
  });

  outstandingFences_.clear();
  stagingBufferFrontOffset_ = 0;
  bufferCapacity_ = stagingBufferSize_;
}

} // namespace vulkan
} // namespace lvk
