/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <igl/IGLFolly.h>
#include <igl/webgpu/Buffer.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief Recycles the chunks of uniform arenas. Owned by WebGPUContext.
///
/// wgpuQueueWriteBuffer() is ordered with queue submissions, so a chunk can be handed to another
/// arena as soon as the command buffer that used it has been submitted: work submitted earlier
/// still reads the old contents.
class UniformArenaPool final {
 public:
  explicit UniformArenaPool(WebGPUContext& ctx) : ctx_(ctx) {}
  ~UniformArenaPool();

  UniformArenaPool(const UniformArenaPool&) = delete;
  UniformArenaPool& operator=(const UniformArenaPool&) = delete;
  UniformArenaPool(UniformArenaPool&&) = delete;
  UniformArenaPool& operator=(UniformArenaPool&&) = delete;

  [[nodiscard]] std::unique_ptr<Buffer> acquire();
  void release(std::unique_ptr<Buffer> chunk);

  [[nodiscard]] size_t getCreationCount() const noexcept {
    return creationCount_;
  }

 private:
  WebGPUContext& ctx_;
  std::vector<std::unique_ptr<Buffer>> free_;
  size_t creationCount_ = 0;
};

/// @brief Per-command-buffer storage for bindBytes(): 256-byte aligned slices of 64 KiB chunks.
///
/// Data is staged on the CPU and written with one wgpuQueueWriteBuffer() per chunk when the
/// command buffer is submitted. Uniform bindings use dynamic offsets, so slices of one chunk share
/// a bind group.
class UniformArena final {
 public:
  static constexpr size_t kChunkSize = 64 * 1024;
  static constexpr size_t kAlignment = 256;
  /// Largest bindBytes() payload (DeviceFeatureLimits::MaxBindBytesBytes).
  static constexpr size_t kMaxAllocationSize = 4096;

  struct Slice {
    Buffer* IGL_NULLABLE buffer = nullptr;
    size_t offset = 0;
    size_t size = 0;
  };

  explicit UniformArena(WebGPUContext& ctx) : ctx_(ctx) {}
  ~UniformArena();

  UniformArena(const UniformArena&) = delete;
  UniformArena& operator=(const UniformArena&) = delete;
  UniformArena(UniformArena&&) = delete;
  UniformArena& operator=(UniformArena&&) = delete;

  /// Copies `length` bytes into a new slice; the slice's size is `length` rounded up to 16 bytes.
  /// Returns an empty slice when `length` is 0 or above kMaxAllocationSize.
  [[nodiscard]] Slice allocate(const void* IGL_NONNULL data, size_t length);
  /// Writes the staged data to the GPU; call before the command buffer is submitted.
  void flush();
  /// Returns the chunks to the pool; call once the command buffer was submitted or dropped.
  void releaseChunks();

 private:
  struct Chunk {
    std::unique_ptr<Buffer> buffer;
    std::vector<uint8_t> staging;
    size_t used = 0;
  };

  WebGPUContext& ctx_;
  std::vector<Chunk> chunks_;
};

} // namespace igl::webgpu
