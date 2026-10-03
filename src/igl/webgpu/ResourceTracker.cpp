/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/ResourceTracker.h>

#include <algorithm>
#include <utility>

namespace igl::webgpu {

namespace {

void destroy(Handle<WGPUBuffer>& buffer) {
  if (buffer) {
    wgpuBufferDestroy(buffer.get());
    buffer = nullptr;
  }
}

void destroy(Handle<WGPUTexture>& texture) {
  if (texture) {
    wgpuTextureDestroy(texture.get());
    texture = nullptr;
  }
}

} // namespace

ResourceTracker::~ResourceTracker() {
  IGL_DEBUG_ASSERT(openSerials_.empty(), "Command buffers outlive the device");
  // Only released: an open command buffer may still hold these.
  retired_.clear();
}

uint64_t ResourceTracker::openCommandBuffer() {
  const uint64_t serial = nextSerial_++;
  openSerials_.insert(serial);
  return serial;
}

void ResourceTracker::closeCommandBuffer(uint64_t serial) {
  if (openSerials_.erase(serial) != 0) {
    collect();
  }
}

bool ResourceTracker::isInUse(uint64_t lastUseSerial) const noexcept {
  return lastUseSerial != 0 && !openSerials_.empty() && *openSerials_.begin() <= lastUseSerial;
}

void ResourceTracker::retire(Handle<WGPUBuffer> buffer, uint64_t lastUseSerial) {
  if (!buffer) {
    return;
  }
  if (isInUse(lastUseSerial)) {
    retired_.push_back(
        {.buffer = std::move(buffer), .texture = {}, .lastUseSerial = lastUseSerial});
    return;
  }
  destroy(buffer);
}

void ResourceTracker::retire(Handle<WGPUTexture> texture, uint64_t lastUseSerial) {
  if (!texture) {
    return;
  }
  if (isInUse(lastUseSerial)) {
    retired_.push_back(
        {.buffer = {}, .texture = std::move(texture), .lastUseSerial = lastUseSerial});
    return;
  }
  destroy(texture);
}

void ResourceTracker::collect() {
  const auto firstInUse =
      std::partition(retired_.begin(), retired_.end(), [this](const RetiredResource& r) {
        return !isInUse(r.lastUseSerial);
      });
  for (auto it = retired_.begin(); it != firstInUse; ++it) {
    destroy(it->buffer);
    destroy(it->texture);
  }
  retired_.erase(retired_.begin(), firstInUse);
}

} // namespace igl::webgpu
