/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <unordered_map>

#include <igl/vulkan/Common.h>
#include <lvk/vulkan/VulkanUtils.h>

namespace lvk {
namespace vulkan {

class VulkanBuffer;
class VulkanContext;
class VulkanImage;
class VulkanImmediateCommands;

class VulkanStagingDevice final {
 public:
  explicit VulkanStagingDevice(VulkanContext& ctx);
  ~VulkanStagingDevice();

  VulkanStagingDevice(const VulkanStagingDevice&) = delete;
  VulkanStagingDevice& operator=(const VulkanStagingDevice&) = delete;

  void bufferSubData(VulkanBuffer& buffer, size_t dstOffset, size_t size, const void* data);
  void getBufferSubData(VulkanBuffer& buffer, size_t srcOffset, size_t size, void* data);
  void imageData2D(VulkanImage& image,
                   const VkRect2D& imageRegion,
                   uint32_t baseMipLevel,
                   uint32_t numMipLevels,
                   uint32_t layer,
                   uint32_t numLayers,
                   VkFormat format,
                   const void* data[]);
  void imageData3D(VulkanImage& image, const VkOffset3D& offset, const VkExtent3D& extent, VkFormat format, const void* data);
  void getImageData2D(VkImage srcImage,
                      const uint32_t level,
                      const uint32_t layer,
                      const VkRect2D& imageRegion,
                      VkFormat format,
                      VkImageLayout layout,
                      void* data,
                      uint32_t dataBytesPerRow,
                      bool flipImageVertical);

 private:
  struct MemoryRegionDesc {
    uint32_t srcOffset_ = 0;
    uint32_t alignedSize_ = 0;
  };

  uint32_t getAlignedSize(uint32_t size) const;
  MemoryRegionDesc getNextFreeOffset(uint32_t size);
  void flushOutstandingFences();

 private:
  VulkanContext& ctx_;
  BufferHandle stagingBuffer_;
  std::unique_ptr<VulkanImmediateCommands> immediate_;
  uint32_t stagingBufferFrontOffset_ = 0;
  uint32_t stagingBufferAlignment_ = 16; // updated to support BC7 compressed image
  uint32_t stagingBufferSize_;
  uint32_t bufferCapacity_;
  std::unordered_map<uint64_t, MemoryRegionDesc> outstandingFences_;
};

} // namespace vulkan
} // namespace lvk
