/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <webgpu/webgpu.h>
#include <igl/Common.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;
struct ReadbackMapState;

/// @brief A region of one mip level and array layer to read back.
struct TextureReadbackDesc {
  WGPUTexture IGL_NULLABLE texture = nullptr;
  WGPUTextureAspect aspect = WGPUTextureAspect_All;
  uint32_t mipLevel = 0;
  uint32_t layer = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bytesPerTexel = 0;
  /// Row pitch of the destination; 0 means tightly packed.
  size_t dstBytesPerRow = 0;
  /// Stores the bottom row first, matching IGL's bottom-up readback convention.
  bool flipVertically = true;
};

/// Copies a texture region through a 256-byte-aligned staging buffer, waits for it and writes the
/// rows to `dst` without padding. Channels are not swizzled (BGRA stays BGRA).
[[nodiscard]] Result readTexture(const WebGPUContext& ctx,
                                 const TextureReadbackDesc& desc,
                                 void* IGL_NONNULL dst);

/// Copies `size` bytes at `offset` of `buffer` to `dst` and waits for them. `offset` and `size`
/// must be multiples of 4 and `buffer` must have CopySrc usage.
[[nodiscard]] Result readBuffer(const WebGPUContext& ctx,
                                WGPUBuffer IGL_NULLABLE buffer,
                                uint64_t offset,
                                uint64_t size,
                                void* IGL_NONNULL dst);

/// @brief A copy into a staging buffer that is submitted now and mapped later, so callers can poll
/// for it instead of waiting (browser builds without JSPI cannot wait). The building block of
/// AsyncTextureReadback and AsyncBufferReadback.
class AsyncMap final {
 public:
  AsyncMap() = default;
  ~AsyncMap();
  AsyncMap(const AsyncMap&) = delete;
  AsyncMap& operator=(const AsyncMap&) = delete;
  AsyncMap(AsyncMap&&) = delete;
  AsyncMap& operator=(AsyncMap&&) = delete;

  /// Creates a `stagingSize`-byte staging buffer, submits the copy that `encode` records into it
  /// and requests its mapping, dropping any map in progress. `ctx` must outlive the map.
  [[nodiscard]] Result begin(const WebGPUContext& ctx,
                             uint64_t stagingSize,
                             const std::function<void(WGPUCommandEncoder, WGPUBuffer)>& encode);
  /// Processes pending WebGPU events without waiting; returns whether the mapping has completed.
  [[nodiscard]] bool poll();
  /// Waits for the mapping to complete.
  [[nodiscard]] Result wait();
  /// Hands the mapped bytes to `consume` and ends the map. The mapping must have completed.
  [[nodiscard]] Result consume(const std::function<void(const uint8_t*)>& consume);

  [[nodiscard]] bool isPending() const noexcept {
    return state_ != nullptr;
  }

 private:
  void reset();

  const WebGPUContext* IGL_NULLABLE ctx_ = nullptr;
  uint64_t stagingSize_ = 0;
  Handle<WGPUBuffer> staging_;
  WGPUFuture future_ = {};
  std::shared_ptr<ReadbackMapState> state_;
};

/// @brief An asynchronous readTexture().
class AsyncTextureReadback final {
 public:
  /// Submits the copy described by `desc` and requests its mapping, dropping any readback in
  /// progress. `ctx` must outlive the readback.
  [[nodiscard]] Result begin(const WebGPUContext& ctx, const TextureReadbackDesc& desc);
  /// See AsyncMap::poll().
  [[nodiscard]] bool poll() {
    return map_.poll();
  }
  /// See AsyncMap::wait().
  [[nodiscard]] Result wait() {
    return map_.wait();
  }
  /// Writes the rows to `dst` like readTexture() and ends the readback. The mapping must have
  /// completed (poll() or wait()).
  [[nodiscard]] Result copyTo(void* IGL_NONNULL dst);
  /// copyTo() that fails with ArgumentOutOfRange instead of writing past `dstSize` bytes.
  [[nodiscard]] Result copyTo(void* IGL_NONNULL dst, size_t dstSize);

  [[nodiscard]] bool isPending() const noexcept {
    return map_.isPending();
  }

 private:
  TextureReadbackDesc desc_;
  AsyncMap map_;
};

/// @brief An asynchronous readBuffer().
class AsyncBufferReadback final {
 public:
  /// Submits a copy of `size` bytes at `offset` of `buffer` (multiples of 4) and requests its
  /// mapping, dropping any readback in progress. `ctx` must outlive the readback.
  [[nodiscard]] Result begin(const WebGPUContext& ctx,
                             WGPUBuffer IGL_NULLABLE buffer,
                             uint64_t offset,
                             uint64_t size);
  /// See AsyncMap::poll().
  [[nodiscard]] bool poll() {
    return map_.poll();
  }
  /// See AsyncMap::wait().
  [[nodiscard]] Result wait() {
    return map_.wait();
  }
  /// Writes the bytes to `dst` and ends the readback. The mapping must have completed.
  [[nodiscard]] Result copyTo(void* IGL_NONNULL dst);

  [[nodiscard]] bool isPending() const noexcept {
    return map_.isPending();
  }
  [[nodiscard]] uint64_t getSize() const noexcept {
    return size_;
  }

 private:
  uint64_t size_ = 0;
  AsyncMap map_;
};

} // namespace igl::webgpu
