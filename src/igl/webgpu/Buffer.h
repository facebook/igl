/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <igl/Buffer.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief Implements the igl::IBuffer interface for WebGPU.
///
/// WebGPU buffers cannot be mapped while the GPU may use them, so every buffer keeps a CPU shadow.
/// upload() and unmap() patch the shadow and write the touched 4-byte-aligned words with
/// wgpuQueueWriteBuffer(). GPU writes (copies into the buffer) mark the shadow stale; map() and
/// unaligned uploads then read the buffer back first.
class Buffer final : public IBuffer {
 public:
  [[nodiscard]] static std::unique_ptr<Buffer> create(WebGPUContext& ctx,
                                                      const BufferDesc& desc,
                                                      Result* IGL_NULLABLE outResult);
  ~Buffer() override;

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer(Buffer&&) = delete;
  Buffer& operator=(Buffer&&) = delete;

  Result upload(const void* IGL_NULLABLE data, const BufferRange& range) override;
  void* IGL_NULLABLE map(const BufferRange& range, Result* IGL_NULLABLE outResult) override;
  void unmap() override;

  [[nodiscard]] BufferDesc::BufferAPIHint requestedApiHints() const noexcept override;
  [[nodiscard]] BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override;
  [[nodiscard]] ResourceStorage storage() const noexcept override;
  [[nodiscard]] size_t getSizeInBytes() const override;
  [[nodiscard]] uint64_t gpuAddress(size_t offset = 0) const override;
  [[nodiscard]] BufferDesc::BufferType getBufferType() const override;

  [[nodiscard]] WGPUBuffer IGL_NULLABLE getWGPUBuffer() const noexcept {
    return buffer_.get();
  }
  /// Size of the WebGPU buffer: the requested length rounded up to 4 bytes.
  [[nodiscard]] uint64_t getAllocatedSize() const noexcept {
    return shadow_.size();
  }

  /// Records that the command buffer with `serial` uses this buffer, writing to it if `gpuWrite`.
  void recordUse(uint64_t serial, bool gpuWrite) noexcept;
  [[nodiscard]] uint64_t getResourceId() const noexcept {
    return resourceId_;
  }
  [[nodiscard]] uint64_t getLastUseSerial() const noexcept {
    return lastUseSerial_;
  }
  /// Whether the GPU may have written data the shadow has not seen yet.
  [[nodiscard]] bool isShadowStale() const noexcept {
    return gpuWriteSerial_ != 0;
  }

 private:
  Buffer(WebGPUContext& ctx, const BufferDesc& desc, Handle<WGPUBuffer> buffer);

  [[nodiscard]] Result refreshShadow();
  void writeShadowRange(size_t offset, size_t size);

  WebGPUContext& ctx_;
  const BufferDesc::BufferType type_;
  const BufferDesc::BufferAPIHint hint_;
  const ResourceStorage storage_;
  const size_t length_;
  Handle<WGPUBuffer> buffer_;
  const uint64_t resourceId_ = allocateResourceId();
  std::vector<uint8_t> shadow_;
  uint64_t lastUseSerial_ = 0;
  // Serial of the last command buffer that wrote to the buffer and has not been read back yet.
  uint64_t gpuWriteSerial_ = 0;
  std::optional<BufferRange> mappedRange_;
};

} // namespace igl::webgpu
