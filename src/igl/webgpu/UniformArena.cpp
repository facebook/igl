/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/UniformArena.h>

#include <cstring>
#include <utility>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

UniformArenaPool::~UniformArenaPool() = default;

std::unique_ptr<Buffer> UniformArenaPool::acquire() {
  if (!free_.empty()) {
    std::unique_ptr<Buffer> chunk = std::move(free_.back());
    free_.pop_back();
    return chunk;
  }
  Result result;
  auto chunk = Buffer::create(
      ctx_,
      {.type = BufferDesc::BufferTypeBits::Uniform | BufferDesc::BufferTypeBits::Storage,
       .length = UniformArena::kChunkSize,
       .debugName = "igl.webgpu.uniformArena"},
      &result);
  if (!result.isOk()) {
    IGL_LOG_ERROR("Cannot create a uniform arena chunk: %s\n", result.message.c_str());
    return nullptr;
  }
  ++creationCount_;
  return chunk;
}

void UniformArenaPool::release(std::unique_ptr<Buffer> chunk) {
  if (chunk) {
    free_.push_back(std::move(chunk));
  }
}

UniformArena::~UniformArena() {
  releaseChunks();
}

void UniformArena::releaseChunks() {
  if (chunks_.empty()) {
    return;
  }
  UniformArenaPool& pool = ctx_.getUniformArenaPool();
  for (Chunk& chunk : chunks_) {
    pool.release(std::move(chunk.buffer));
  }
  chunks_.clear();
}

UniformArena::Slice UniformArena::allocate(const void* IGL_NONNULL data, size_t length) {
  if (length == 0 || length > kMaxAllocationSize) {
    return {};
  }
  const size_t reserved = (length + kAlignment - 1) / kAlignment * kAlignment;
  if (chunks_.empty() || chunks_.back().used + reserved > kChunkSize) {
    std::unique_ptr<Buffer> buffer = ctx_.getUniformArenaPool().acquire();
    if (!buffer) {
      return {};
    }
    chunks_.push_back({.buffer = std::move(buffer), .staging = std::vector<uint8_t>(kChunkSize)});
  }
  Chunk& chunk = chunks_.back();
  const size_t offset = chunk.used;
  std::memcpy(chunk.staging.data() + offset, data, length);
  chunk.used += reserved;
  return {.buffer = chunk.buffer.get(), .offset = offset, .size = (length + 15) & ~size_t{15}};
}

void UniformArena::flush() {
  for (Chunk& chunk : chunks_) {
    if (chunk.used == 0) {
      continue;
    }
    const Result result = chunk.buffer->upload(chunk.staging.data(), {chunk.used, 0});
    if (!result.isOk()) {
      IGL_LOG_ERROR("Uniform arena upload failed: %s\n", result.message.c_str());
    }
    chunk.used = 0;
  }
}

} // namespace igl::webgpu
