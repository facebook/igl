/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <deque>
#include <memory>
#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl {
namespace vulkan {

class VulkanBuffer;
class VulkanContext;
class VulkanImage;

class VulkanStagingDevice final {
 public:
  explicit VulkanStagingDevice(VulkanContext& ctx);
  ~VulkanStagingDevice() = default;

  VulkanStagingDevice(const VulkanStagingDevice&) = delete;
  VulkanStagingDevice& operator=(const VulkanStagingDevice&) = delete;

  void bufferSubData(VulkanBuffer& buffer, size_t dstOffset, size_t size, const void* data);
  void getBufferSubData(VulkanBuffer& buffer, size_t srcOffset, size_t size, void* data);
  void imageData(VulkanImage& image,
                 TextureType type,
                 const TextureRangeDesc& range,
                 const TextureFormatProperties& properties,
                 uint32_t bytesPerRow,
                 const void* data);
  void getImageData2D(VkImage srcImage,
                      const uint32_t level,
                      const uint32_t layer,
                      const VkRect2D& imageRegion,
                      TextureFormatProperties properties,
                      VkFormat format,
                      VkImageLayout layout,
                      void* data,
                      uint32_t bytesPerRow,
                      bool flipImageVertical);

 private:
  struct MemoryRegion {
    uint32_t offset = 0u;
    uint32_t size = 0u;
    VulkanImmediateCommands::SubmitHandle handle;
  };

  /**
   * @brief Searches for an available block in the staging buffer that is as large as the size
   * requested. If the only contiguous block of memory available is smaller than the requested size,
   * the function returns the amount of memory it was able to find.
   *
   * @return The offset of the free memory block on the staging buffer and the size of the block
   * found.
   */
  MemoryRegion nextFreeBlock(uint32_t size);

  uint32_t getAlignedSize(uint32_t size) const;

  /// @brief Waits for all memory blocks to become available and resets the staging device's
  /// internal state
  void waitAndReset();

 private:
  VulkanContext& ctx_;
  std::shared_ptr<VulkanBuffer> stagingBuffer_;
  std::unique_ptr<VulkanImmediateCommands> immediate_;
  uint32_t stagingBufferSize_;
  uint32_t bufferCapacity_;

  /**
   * @brief Stores the used and unused blocks of memory in the staging buffer. There is no
   * distinction between used and unused blocks in the deque, as we always `wait` on each block
   * before using them. Older blocks are stored at the front of the deque, while used/busy
   * ones are stored at the end. Older blocks have a higher chance of being unused (not waiting for
   * the associated command buffer to finish)
   */
  std::deque<MemoryRegion> regions_;
};

} // namespace vulkan
} // namespace igl
