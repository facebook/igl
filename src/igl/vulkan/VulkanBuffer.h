/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <lvk/vulkan/VulkanUtils.h>

namespace lvk {
namespace vulkan {

class VulkanContext;

class VulkanBuffer final {
 public:
  VulkanBuffer() = default;
  VulkanBuffer(VulkanContext* ctx,
               VkDevice device,
               VkDeviceSize bufferSize,
               VkBufferUsageFlags usageFlags,
               VkMemoryPropertyFlags memFlags,
               const char* debugName = nullptr);
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer&) = delete;
  VulkanBuffer& operator=(const VulkanBuffer&) = delete;

  VulkanBuffer(VulkanBuffer&& other);
  VulkanBuffer& operator=(VulkanBuffer&& other);

  void bufferSubData(size_t offset, size_t size, const void* data);
  void getBufferSubData(size_t offset, size_t size, void* data);
  [[nodiscard]] uint8_t* getMappedPtr() const {
    return static_cast<uint8_t*>(mappedPtr_);
  }
  bool isMapped() const {
    return mappedPtr_ != nullptr;
  }
  void flushMappedMemory(VkDeviceSize offset, VkDeviceSize size) const;
  VkBuffer getVkBuffer() const {
    return vkBuffer_;
  }
  VkDeviceAddress getVkDeviceAddress() const {
    return vkDeviceAddress_;
  }
  VkDeviceSize getSize() const {
    return bufferSize_;
  }
  VkBufferUsageFlags getUsageFlags() const {
    return usageFlags_;
  }

 private:
  VulkanContext* ctx_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;
  VkBuffer vkBuffer_ = VK_NULL_HANDLE;
  VkDeviceMemory vkMemory_ = VK_NULL_HANDLE;
  VmaAllocationCreateInfo vmaAllocInfo_ = {};
  VmaAllocation vmaAllocation_ = VK_NULL_HANDLE;
  VkDeviceAddress vkDeviceAddress_ = 0;
  VkDeviceSize bufferSize_ = 0;
  VkBufferUsageFlags usageFlags_ = 0;
  VkMemoryPropertyFlags memFlags_ = 0;
  void* mappedPtr_ = nullptr;
};

} // namespace vulkan
} // namespace lvk
