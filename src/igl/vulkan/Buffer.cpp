/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstring>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanStagingDevice.h>

namespace igl::vulkan {

Buffer::Buffer(const igl::vulkan::Device& device) : device_(device) {}

Result Buffer::create(const BufferDesc& desc) {
  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  if (!ctx.useStaging_ && (desc_.storage == ResourceStorage::Private)) {
    desc_.storage = ResourceStorage::Shared;
  }

  /* Use staging device to transfer data into the buffer when the storage is private to the device
   */
  VkBufferUsageFlags usageFlags =
      (desc_.storage == ResourceStorage::Private)
          ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
          : 0;

  if (desc_.type == 0) {
    return Result(Result::Code::InvalidOperation, "Invalid buffer type");
  }

  if (desc_.type & BufferDesc::BufferTypeBits::Index) {
    usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (desc_.type & BufferDesc::BufferTypeBits::Vertex) {
    usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (desc_.type & BufferDesc::BufferTypeBits::Uniform) {
    usageFlags |=
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc_.type & BufferDesc::BufferTypeBits::Storage) {
    usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc_.type & BufferDesc::BufferTypeBits::Indirect) {
    usageFlags |=
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  const VkMemoryPropertyFlags memFlags = resourceStorageToVkMemoryPropertyFlags(desc_.storage);

  Result result;
  buffer_ = ctx.createBuffer(desc_.length, usageFlags, memFlags, &result, desc_.debugName);

  IGL_VERIFY(result.isOk());

  return result;
}

igl::Result Buffer::upload(const void* data, size_t size, size_t offset) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(data)) {
    return igl::Result();
  }

  if (!IGL_VERIFY(offset + size <= desc_.length)) {
    return igl::Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  // use staging to upload data to device-local buffers
  const VulkanContext& ctx = device_.getVulkanContext();
  ctx.stagingDevice_->bufferSubData(*buffer_, offset, size, data);
  return igl::Result();
}

uint64_t Buffer::gpuAddress(size_t offset) const {
  IGL_ASSERT_MSG((offset & 7) == 0,
                 "Buffer offset must be 8 bytes aligned as per GLSL_EXT_buffer_reference spec.");

  return (uint64_t)buffer_->getVkDeviceAddress() + offset;
}

VkBuffer Buffer::getVkBuffer() const {
  return buffer_->getVkBuffer();
}

uint8_t* Buffer::getMappedPtr() const {
  return buffer_->isMapped() ? buffer_->getMappedPtr() : nullptr;
}

} // namespace igl::vulkan
