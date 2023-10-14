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
#include <igl/vulkan/SyncManager.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanStagingDevice.h>

#include <igl/IGLSafeC.h>
#include <memory>

namespace igl {

namespace vulkan {

Buffer::Buffer(const igl::vulkan::Device& device) : device_(device) {}

Result Buffer::create(const BufferDesc& desc) {
  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  if (!ctx.useStagingForBuffers_ && (desc_.storage == ResourceStorage::Private)) {
    desc_.storage = ResourceStorage::Shared;
  }

  /* Use staging device to transfer data into the buffer when the storage is private to the device
   */
  VkBufferUsageFlags usageFlags =
      (desc_.storage == ResourceStorage::Private)
          ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
          : 0;

  const VkBufferUsageFlags optionalBDA =
      ctx.config_.enableBufferDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR : 0;

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
    usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | optionalBDA;
  }

  if (desc_.type & BufferDesc::BufferTypeBits::Storage) {
    usageFlags |=
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | optionalBDA;
  }

  if (desc_.type & BufferDesc::BufferTypeBits::Indirect) {
    usageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | optionalBDA;
  }

  const VkMemoryPropertyFlags memFlags = resourceStorageToVkMemoryPropertyFlags(desc_.storage);

  // Store the flag that determines if this buffer contains sub-allocations (i.e. is a ring-buffer)
  isRingBuffer_ = ((desc_.hint & BufferDesc::BufferAPIHintBits::Ring) != 0);

  const auto numBuffers =
      isRingBuffer_ ? device_.getVulkanContext().syncManager_->maxResourceCount() : 1u;

  if (isRingBuffer_) {
    // Resize the local copy of the data
    localData_ = std::make_unique<uint8_t[]>(desc_.length);
  }

  buffers_.reserve(numBuffers);
  bufferPatches_.resize(numBuffers, BufferRange());
  Result result;
  for (size_t bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex) {
    std::string bufferName = desc_.debugName + " - sub-buffer " + std::to_string(bufferIndex);
    buffers_.emplace_back(
        ctx.createBuffer(desc_.length, usageFlags, memFlags, &result, bufferName.c_str()));
    IGL_VERIFY(result.isOk());
  }

  return result;
}

const std::shared_ptr<VulkanBuffer>& Buffer::currentVulkanBuffer() const {
  IGL_ASSERT_MSG(!buffers_.empty(), "There are no sub-allocations available for this buffer");
  return buffers_[isRingBuffer_ ? device_.getVulkanContext().syncManager_->currentIndex() : 0u];
}

BufferRange Buffer::getUpdateRange() const {
  size_t start = std::numeric_limits<size_t>::max();
  size_t end = 0;

  for (uint32_t i = 0; i < buffers_.size(); ++i) {
    const auto& bufferPatch = bufferPatches_[i];
    // skip this buffer, if update size is zero
    if (bufferPatch.size == 0) {
      continue;
    }
    end = std::max(end, bufferPatch.offset + bufferPatch.size);
    start = std::min(start, bufferPatch.offset);
  }

  // If there is no new data, return an empty range to indicate that no data is available
  if (start == std::numeric_limits<size_t>::max()) {
    return BufferRange();
  }

  return BufferRange(end - start, start);
}

void Buffer::extendUpdateRange(uint32_t ringBufferIndex, const BufferRange& range) {
  auto& bufferPatch = bufferPatches_[ringBufferIndex];
  const size_t start = std::min(bufferPatch.offset, range.offset);
  const size_t end = std::max(bufferPatch.offset + bufferPatch.size, range.size + range.offset);

  bufferPatch.offset = start;
  bufferPatch.size = end - start;
}

void Buffer::resetUpdateRange(uint32_t ringBufferIndex, const BufferRange& range) {
  bufferPatches_[ringBufferIndex] = range;
}

igl::Result Buffer::upload(const void* data, const BufferRange& range) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(data)) {
    return igl::Result();
  }

  if (!IGL_VERIFY(range.offset + range.size <= desc_.length)) {
    return igl::Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  // To handle an upload to a ring-buffer, we update the local copy first and upload the entire
  // local data to the device below
  const VulkanContext& ctx = device_.getVulkanContext();
  if (isRingBuffer_) {
    // update local data copy
    checked_memcpy(localData_.get() + range.offset, range.size, (void*)data, range.size);
    // get the current ring buffer index
    const auto currentBufferIndex = device_.getVulkanContext().syncManager_->currentIndex();
    BufferRange currentUpdateRange = range;
    if (currentBufferIndex != previousBufferIndex_) {
      // if the index has changed update the index
      previousBufferIndex_ = currentBufferIndex;
      // reset update range at the current index, using input range
      resetUpdateRange(currentBufferIndex, range);
      // get full update range for this index, based on updates made in all the other buffers
      currentUpdateRange = getUpdateRange();
    } else {
      // increase buffer update range at the current index, based on new range
      extendUpdateRange(currentBufferIndex, range);
    }
    // use staging to upload data to device-local buffers
    ctx.stagingDevice_->bufferSubData(*currentVulkanBuffer(),
                                      currentUpdateRange.offset,
                                      currentUpdateRange.size,
                                      localData_.get() + currentUpdateRange.offset);
  } else {
    // use staging to upload data to device-local buffers
    ctx.stagingDevice_->bufferSubData(*currentVulkanBuffer(), range.offset, range.size, data);
  }
  return igl::Result();
}

size_t Buffer::getSizeInBytes() const {
  return desc_.length;
}

uint64_t Buffer::gpuAddress(size_t offset) const {
  IGL_ASSERT_MSG((offset & 7) == 0,
                 "Buffer offset must be 8 bytes aligned as per GLSL_EXT_buffer_reference spec.");

  return (uint64_t)currentVulkanBuffer()->getVkDeviceAddress() + offset;
}

VkBuffer Buffer::getVkBuffer() const {
  return currentVulkanBuffer()->getVkBuffer();
}

void* Buffer::map(const BufferRange& range, igl::Result* outResult) {
  IGL_ASSERT_MSG(!isRingBuffer_, "Buffer::map() operation not supported for ring buffer");

  // Sanity check
  if ((range.size > desc_.length) || (range.offset > desc_.length - range.size)) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Range exceeds buffer length");
    // @fb-only
    // @lint-ignore CLANGTIDY
    return nullptr;
  }

  // If the buffer is currently mapped, then unmap it first
  if (mappedRange_.size &&
      (mappedRange_.size != range.size || mappedRange_.offset != range.offset)) {
    IGL_ASSERT_MSG(false, "Buffer::map() is called more than once without Buffer::unmap()");
    unmap();
  }

  mappedRange_ = range;

  Result::setOk(outResult);

  const auto& buffer = currentVulkanBuffer();
  if (!buffer->isMapped()) {
    // handle DEVICE_LOCAL buffers
    tmpBuffer_.resize(range.size);
    const VulkanContext& ctx = device_.getVulkanContext();
    ctx.stagingDevice_->getBufferSubData(*buffer, range.offset, range.size, tmpBuffer_.data());
    return tmpBuffer_.data();
  }

  return buffer->getMappedPtr() + range.offset;
}

void Buffer::unmap() {
  IGL_ASSERT_MSG(!isRingBuffer_, "Buffer::unmap() operation not supported for ring buffer");
  IGL_ASSERT_MSG(mappedRange_.size, "Called Buffer::unmap() without Buffer::map()");

  const auto& buffer = currentVulkanBuffer();
  const BufferRange range(tmpBuffer_.size(), mappedRange_.offset);
  if (!buffer->isMapped()) {
    // handle DEVICE_LOCAL buffers
    upload(tmpBuffer_.data(), range);
  } else if (!buffer->isCoherentMemory()) {
    buffer->flushMappedMemory(range.offset, range.size);
  }
  mappedRange_.size = 0;
}

BufferDesc::BufferAPIHint Buffer::requestedApiHints() const noexcept {
  return desc_.hint;
}

BufferDesc::BufferAPIHint Buffer::acceptedApiHints() const noexcept {
  if (desc_.type & BufferDesc::BufferTypeBits::Uniform) {
    return BufferDesc::BufferAPIHintBits::UniformBlock;
  }

  return 0;
}

ResourceStorage Buffer::storage() const noexcept {
  return desc_.storage;
}

} // namespace vulkan
} // namespace igl
