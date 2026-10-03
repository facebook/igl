/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

/// @brief Defers Destroy() of retired resources until no open command buffer can reference them.
///
/// Every command buffer gets a serial when it starts recording and stays open until it is submitted
/// or dropped. A resource records the serial of the last command buffer that used it. Destroying a
/// resource used by an open command buffer makes the later submit fail validation, while work that
/// was already submitted keeps the resource alive on its own, so a retired resource is destroyed as
/// soon as no open command buffer with a serial at or below its last use remains.
class ResourceTracker final {
 public:
  ResourceTracker() = default;
  ~ResourceTracker();

  ResourceTracker(const ResourceTracker&) = delete;
  ResourceTracker& operator=(const ResourceTracker&) = delete;
  ResourceTracker(ResourceTracker&&) = delete;
  ResourceTracker& operator=(ResourceTracker&&) = delete;

  /// Returns the serial of a new command buffer; serials start at 1 and increase.
  [[nodiscard]] uint64_t openCommandBuffer();
  /// The command buffer was submitted or dropped.
  void closeCommandBuffer(uint64_t serial);

  /// Takes the last reference of a resource whose most recent use is `lastUseSerial` (0 if never
  /// used by a command buffer).
  void retire(Handle<WGPUBuffer> buffer, uint64_t lastUseSerial);
  void retire(Handle<WGPUTexture> texture, uint64_t lastUseSerial);

  /// Whether the command buffer with `serial` is still recording.
  [[nodiscard]] bool isOpen(uint64_t serial) const noexcept {
    return openSerials_.count(serial) != 0;
  }
  [[nodiscard]] size_t getOpenCommandBufferCount() const noexcept {
    return openSerials_.size();
  }
  [[nodiscard]] size_t getPendingRetirementCount() const noexcept {
    return retired_.size();
  }

 private:
  struct RetiredResource {
    Handle<WGPUBuffer> buffer;
    Handle<WGPUTexture> texture;
    uint64_t lastUseSerial = 0;
  };

  [[nodiscard]] bool isInUse(uint64_t lastUseSerial) const noexcept;
  void collect();

  uint64_t nextSerial_ = 1;
  std::set<uint64_t> openSerials_;
  std::vector<RetiredResource> retired_;
};

} // namespace igl::webgpu
