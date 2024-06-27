/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/Buffer.h>

namespace igl::metal {

class BufferSynchronizationManager {
 public:
  explicit BufferSynchronizationManager(size_t maxInFlightBuffers);

  /**
   * @brief Returns the current inFlight buffer index
   * @return the current inFlight buffer index
   */
  [[nodiscard]] size_t getCurrentInFlightBufferIndex() const noexcept {
    return currentInFlightBufferIndex_;
  }

  /**
   * @brief Returns the max inFlight buffers that are supported by the backend being used
   * InFlight buffers are buffers that can be accessed by GPU aynchronously at any moment.
   * To avoid CPU/GPU synchronization issues (where for ex. CPU could be updating a buffer with data
   * for frame N+1, while GPU is reading the same buffer for rendering frame N), certain backends
   * (like Metal) will support more than one in flight buffers.
   * @return max inFlight buffers
   */
  [[nodiscard]] size_t getMaxInflightBuffers() const noexcept {
    return maxInFlightBuffers_;
  }

  void manageEndOfFrameSync();

  // Upon completion of this command buffer's execution, trigger buffer synchronization.
  void markCommandBufferAsEndOfFrame(const igl::ICommandBuffer& commandBuffer);

 private:
  size_t maxInFlightBuffers_ = 1;
  size_t currentInFlightBufferIndex_ = 0;
  dispatch_semaphore_t frameBoundarySemaphore_;
};

} // namespace igl::metal
