/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>

#include <vector>

namespace igl {
namespace vulkan {

class Device;
class VulkanBuffer;

class Buffer final : public igl::IBuffer {
  friend class Device;
  friend class CommandBuffer;

 public:
  explicit Buffer(const igl::vulkan::Device& device);
  ~Buffer() override = default;

  Result upload(const void* data, size_t size, size_t offset = 0) override;

  uint8_t* getMappedPtr() const override;
  uint64_t gpuAddress(size_t offset) const override;

  VkBuffer getVkBuffer() const;

 private:
  Result create(const BufferDesc& desc);

 private:
  const igl::vulkan::Device& device_;
  BufferDesc desc_;
  std::shared_ptr<VulkanBuffer> buffer_;
};

} // namespace vulkan
} // namespace igl
