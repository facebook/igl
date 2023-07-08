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

  size_t getSizeInBytes() const override;
  uint64_t gpuAddress(size_t offset) const override;

  VkBuffer getVkBuffer() const;
  BufferDesc::BufferType getBufferType() const {
    return desc_.type;
  }

 private:
  Result create(const BufferDesc& desc);

 private:
  const igl::vulkan::Device& device_;
  BufferDesc desc_;
  std::shared_ptr<VulkanBuffer> buffer_;

  // Used for map/unmap API for DEVICE_LOCAL buffers
  std::vector<uint8_t> tmpBuffer_;
  BufferRange mappedRange_ = {};
};

} // namespace vulkan
} // namespace igl
