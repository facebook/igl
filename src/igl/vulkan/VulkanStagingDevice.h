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

  [[nodiscard]] VkDeviceSize getCurrentStagingBufferSize() const {
    return stagingBufferSize_;
  }
  [[nodiscard]] VkDeviceSize getMaxStagingBufferSize() const {
    return maxStagingBufferSize_;
  }

 private:
  struct MemoryRegion {
    VkDeviceSize offset = 0u;
    VkDeviceSize size = 0u;
    VkDeviceSize alignedSize = 0u;
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
  [[nodiscard]] MemoryRegion nextFreeBlock(VkDeviceSize size);

  [[nodiscard]] VkDeviceSize getAlignedSize(VkDeviceSize size) const;

  /// @brief Waits for all memory blocks to become available and resets the staging device's
  /// internal state
  void waitAndReset();

  /// @brief Returns true if the staging buffer cannot store the size requested
  [[nodiscard]] bool shouldGrowStagingBuffer(VkDeviceSize sizeNeeded) const;

  /// @brief Returns the next size to allocate for the staging buffer given the requested size
  [[nodiscard]] VkDeviceSize nextSize(VkDeviceSize requestedSize) const;

  /// @brief Grows the staging buffer to a size that is at least as large as the requested size
  void growStagingBuffer(VkDeviceSize minimumSize);

 private:
  VulkanContext& ctx_;
  std::shared_ptr<VulkanBuffer> stagingBuffer_;
  std::unique_ptr<VulkanImmediateCommands> immediate_;

  /// @brief Current size of the staging buffer
  VkDeviceSize stagingBufferSize_ = 0;
  /// @brief Maximum staging buffer size, limited by some architectures
  VkDeviceSize maxStagingBufferSize_ = 0;
  /// @brief Used to track the current staging buffer's id. Updated every time the staging buffer
  /// grows, it is used as the debug name for the staging buffer for easily tracking it during
  /// debugging
  uint32_t stagingBufferCounter_ = 0;

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
