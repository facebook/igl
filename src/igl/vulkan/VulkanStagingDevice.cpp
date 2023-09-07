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

  // Use default value of 256 MB, and clamp it to the max limits
  stagingBufferSize_ = std::min(limits.maxStorageBufferRange, 256u * 1024u * 1024u);

  bufferCapacity_ = stagingBufferSize_;

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

  size_t chunkDstOffset = dstOffset;
  void* copyData = const_cast<void*>(data);

  while (size) {
    // get next staging buffer free offset
    const MemoryRegionDesc desc = getNextFreeOffset((uint32_t)size);
    const uint32_t chunkSize = std::min((uint32_t)size, desc.alignedSize_);

    // copy data into staging buffer
    stagingBuffer_->bufferSubData(desc.srcOffset_, chunkSize, copyData);

    // do the transfer
    const VkBufferCopy copy = {desc.srcOffset_, chunkDstOffset, chunkSize};

    auto& wrapper = immediate_->acquire();
    vkCmdCopyBuffer(wrapper.cmdBuf_, stagingBuffer_->getVkBuffer(), buffer.getVkBuffer(), 1, &copy);
    const VulkanSubmitHandle fenceId = immediate_->submit(wrapper);
    outstandingFences_[fenceId.handle()] = desc;

    size -= chunkSize;
    copyData = (uint8_t*)copyData + chunkSize;
    chunkDstOffset += chunkSize;
  }
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
    // get next staging buffer free offset
    const MemoryRegionDesc desc = getNextFreeOffset((uint32_t)size);
    const uint32_t chunkSize = std::min((uint32_t)size, desc.alignedSize_);

    // do the transfer
    const VkBufferCopy copy = {chunkSrcOffset, desc.srcOffset_, chunkSize};

    auto& wrapper = immediate_->acquire();

    vkCmdCopyBuffer(wrapper.cmdBuf_, buffer.getVkBuffer(), stagingBuffer_->getVkBuffer(), 1, &copy);

    VulkanSubmitHandle fenceId = immediate_->submit(wrapper);
    outstandingFences_[fenceId.handle()] = desc;

    // Wait for command to finish
    flushOutstandingFences();

    // copy data into data
    const uint8_t* src = stagingBuffer_->getMappedPtr() + desc.srcOffset_;
    checked_memcpy(dstData, size - chunkSrcOffset, src, chunkSize);

    size -= chunkSize;
    dstData = (uint8_t*)dstData + chunkSize;
    chunkSrcOffset += chunkSize;
  }
}

void VulkanStagingDevice::imageData(VulkanImage& image,
                                    TextureType type,
                                    const TextureRangeDesc& range,
                                    const TextureFormatProperties& properties,
                                    uint32_t bytesPerRow,
                                    const void* data) {
  IGL_PROFILER_FUNCTION();
  std::unique_ptr<uint8_t[]> repackedData = nullptr;

  // Vulkan only handles cases where row lengths are multiples of texel block size.
  // Must repack the data if the source data does not conform to this.
  if (bytesPerRow != 0 && bytesPerRow % properties.bytesPerBlock != 0) {
    // Must repack the data.
    repackedData = std::make_unique<uint8_t[]>(properties.getBytesPerRange(range));
    ITexture::repackData(
        properties, range, static_cast<const uint8_t*>(data), bytesPerRow, repackedData.get(), 0);
    bytesPerRow = 0;
    data = repackedData.get();
  }

  const uint32_t storageSize =
      static_cast<uint32_t>(properties.getBytesPerRange(range, bytesPerRow));

  IGL_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(storageSize);

  // currently, no support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storageSize, we will wait for gpu idle and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }

  IGL_ASSERT(desc.alignedSize_ >= storageSize);

  // 1. Copy the pixel data into the host visible staging buffer
  stagingBuffer_->bufferSubData(desc.srcOffset_, storageSize, data);

  auto& wrapper = immediate_->acquire();

  const uint32_t initialLayer = getVkLayer(type, range.face, range.layer);
  const uint32_t numLayers = getVkLayer(type, range.numFaces, range.numLayers);

  std::vector<VkBufferImageCopy> copyRegions;
  copyRegions.reserve(range.numMipLevels);

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
      copyRegions.emplace_back(
          ivkGetBufferImageCopy2D(desc.srcOffset_ + offset,
                                  texelsPerRow,
                                  region,
                                  VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT,
                                                           static_cast<uint32_t>(mipLevel),
                                                           initialLayer,
                                                           numLayers}));
    } else {
      copyRegions.emplace_back(
          ivkGetBufferImageCopy3D(desc.srcOffset_ + offset,
                                  texelsPerRow,
                                  VkOffset3D{static_cast<int32_t>(mipRange.x),
                                             static_cast<int32_t>(mipRange.y),
                                             static_cast<int32_t>(mipRange.z)},
                                  VkExtent3D{static_cast<uint32_t>(mipRange.width),
                                             static_cast<uint32_t>(mipRange.height),
                                             static_cast<uint32_t>(mipRange.depth)},
                                  VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT,
                                                           static_cast<uint32_t>(mipLevel),
                                                           initialLayer,
                                                           numLayers}));
    }
  }

  const auto subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT,
                                                        static_cast<uint32_t>(range.mipLevel),
                                                        static_cast<uint32_t>(range.numMipLevels),
                                                        initialLayer,
                                                        numLayers};
  // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
  ivkImageMemoryBarrier(wrapper.cmdBuf_,
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
  vkCmdCopyBufferToImage(wrapper.cmdBuf_,
                         stagingBuffer_->getVkBuffer(),
                         image.getVkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(copyRegions.size()),
                         copyRegions.data());

  // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
  ivkImageMemoryBarrier(wrapper.cmdBuf_,
                        image.getVkImage(),
                        VK_ACCESS_TRANSFER_READ_BIT, // VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        subresourceRange);

  image.imageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VulkanSubmitHandle fenceId = immediate_->submit(wrapper);
  outstandingFences_[fenceId.handle()] = desc;
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
  MemoryRegionDesc desc = getNextFreeOffset(storageSize);

  // If we get smaller buffer size than storageSize, we will wait for gpu idle
  // and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    flushOutstandingFences();
    desc = getNextFreeOffset(storageSize);
  }

  IGL_ASSERT(desc.alignedSize_ >= storageSize);

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
      desc.srcOffset_,
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

  VulkanSubmitHandle fenceId = immediate_->submit(wrapper1);
  outstandingFences_[fenceId.handle()] = desc;

  flushOutstandingFences();

  // 3. Copy data from staging buffer into data
  if (!IGL_VERIFY(stagingBuffer_->getMappedPtr())) {
    return;
  }

  const uint8_t* src = stagingBuffer_->getMappedPtr() + desc.srcOffset_;
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

  fenceId = immediate_->submit(wrapper2);
  outstandingFences_[fenceId.handle()] = desc;
  // the data should be available as we get out of this function
  immediate_->wait(fenceId);
}

uint32_t VulkanStagingDevice::getAlignedSize(uint32_t size) const {
  return (size + stagingBufferAlignment_ - 1) & ~(stagingBufferAlignment_ - 1);
}

VulkanStagingDevice::MemoryRegionDesc VulkanStagingDevice::getNextFreeOffset(uint32_t size) {
  IGL_PROFILER_FUNCTION();
  uint32_t alignedSize = getAlignedSize(size);

  // track maximum previously used region
  MemoryRegionDesc maxRegionDesc;
  VulkanSubmitHandle maxRegionFence = VulkanSubmitHandle();

  // check if we can reuse any of previously used memory region
  for (auto [handle, desc] : outstandingFences_) {
    if (immediate_->isReady(VulkanSubmitHandle(handle))) {
      if (desc.alignedSize_ >= alignedSize) {
        outstandingFences_.erase(handle);
#if IGL_VULKAN_DEBUG_STAGING_DEVICE
        IGL_LOG_INFO("Reusing memory region %u bytes\n", desc.alignedSize_);
#endif
        return desc;
      }

      if (maxRegionDesc.alignedSize_ < desc.alignedSize_) {
        maxRegionDesc = desc;
        maxRegionFence = VulkanSubmitHandle(handle);
      }
    }
  }

  if (!maxRegionFence.empty() && bufferCapacity_ < maxRegionDesc.alignedSize_) {
    outstandingFences_.erase(maxRegionFence.handle());
#if IGL_VULKAN_DEBUG_STAGING_DEVICE
    IGL_LOG_INFO("Reusing memory region %u bytes\n", maxRegionDesc.alignedSize_);
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

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("Allocating new memory region: %u bytes\n", alignedSize);
#endif

  return {srcOffset, alignedSize};
}

void VulkanStagingDevice::flushOutstandingFences() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

#if IGL_VULKAN_DEBUG_STAGING_DEVICE
  IGL_LOG_INFO("StagingDevice - Wait for Idle\n");
#endif
  std::for_each(outstandingFences_.begin(),
                outstandingFences_.end(),
                [this](std::pair<uint64_t, MemoryRegionDesc> const& pair) {
                  immediate_->wait(VulkanSubmitHandle(pair.first));
                });

  outstandingFences_.clear();
  stagingBufferFrontOffset_ = 0;
  bufferCapacity_ = stagingBufferSize_;
}

} // namespace vulkan
} // namespace igl
