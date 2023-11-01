/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/vulkan/Common.h>

namespace igl {
namespace vulkan {

class Device;
class VulkanBuffer;

class Buffer final : public igl::IBuffer {
  friend class Device;

 public:
  explicit Buffer(const igl::vulkan::Device& device);
  ~Buffer() override = default;

  Result upload(const void* data, const BufferRange& range) override;

  void* map(const BufferRange& range, Result* outResult) override;

  void unmap() override;

  BufferDesc::BufferAPIHint requestedApiHints() const noexcept override;
  BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override;
  ResourceStorage storage() const noexcept override;

  size_t getSizeInBytes() const override;
  uint64_t gpuAddress(size_t offset) const override;

  BufferDesc::BufferType getBufferType() const override {
    return desc_.type;
  }

  VkBuffer getVkBuffer() const;

 private:
  Result create(const BufferDesc& desc);
  [[nodiscard]] const std::shared_ptr<VulkanBuffer>& currentVulkanBuffer() const;

 private:
  const igl::vulkan::Device& device_;
  BufferDesc desc_;
  bool isRingBuffer_ = false;
  uint32_t previousBufferIndex_ = UINT32_MAX;
  std::vector<std::shared_ptr<VulkanBuffer>> buffers_;
  std::unique_ptr<uint8_t[]> localData_;
  std::vector<BufferRange> bufferPatches_;

  // Function determines smallest starting and largest ending offset by iterating over all
  // bufferPatches_, and returns it in the form of buffer range
  [[nodiscard]] BufferRange getUpdateRange() const;
  void extendUpdateRange(uint32_t ringBufferIndex, const BufferRange& range);
  void resetUpdateRange(uint32_t ringBufferIndex, const BufferRange& range);

  // Used for map/unmap API for DEVICE_LOCAL buffers
  std::vector<uint8_t> tmpBuffer_;
  BufferRange mappedRange_ = {};
  BufferDesc::BufferAPIHint requestedApiHints_ = 0;
};

} // namespace vulkan
} // namespace igl
