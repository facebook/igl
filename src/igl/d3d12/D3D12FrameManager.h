/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class D3D12Context;

/**
 * @brief Manages frame advancement, fence waiting, and resource cleanup
 *
 * Centralizes the complex logic for:
 * - Waiting for next frame's resources to become available
 * - Pipeline overload protection (ensuring max frames in flight)
 * - Safe command allocator reset after GPU completion
 * - Transient resource cleanup
 * - Descriptor heap reset
 */
class FrameManager final {
 public:
  explicit FrameManager(D3D12Context& context) : context_(context) {}

  /**
   * @brief Advance to next frame with proper synchronization
   *
   * Handles:
   * 1. Calculate next frame index
   * 2. Wait for pipeline overload protection
   * 3. Wait for next frame's resources
   * 4. Update frame index
   * 5. Reset allocator safely
   * 6. Clear transient resources
   * 7. Reset descriptor counters
   *
   * @param currentFenceValue The fence value just signaled
   */
  void advanceFrame(UINT64 currentFenceValue);

 private:
  /**
   * @brief Wait for pipeline to avoid overload (max frames in flight)
   */
  void waitForPipelineSync(UINT64 currentFenceValue);

  /**
   * @brief Wait for specific frame's resources to become available
   * @return true if wait succeeded, false if catastrophic wait failure
   */
  bool waitForFrame(uint32_t frameIndex);

  /**
   * @brief Safely reset command allocator after GPU completion
   */
  void resetAllocator(uint32_t frameIndex);

  /**
   * @brief Clear transient resources from completed frame
   */
  void clearTransientResources(uint32_t frameIndex);

  /**
   * @brief Log and reset descriptor usage counters
   */
  void resetDescriptorCounters(uint32_t frameIndex);

  D3D12Context& context_;
};

} // namespace igl::d3d12
