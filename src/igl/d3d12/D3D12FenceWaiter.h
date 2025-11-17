/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

/**
 * @brief RAII helper for D3D12 fence waiting operations
 *
 * Manages event creation, SetEventOnCompletion, and proper cleanup.
 * Eliminates TOCTOU races by rechecking fence after SetEventOnCompletion.
 *
 * Usage:
 *   FenceWaiter waiter(fence, targetValue);
 *   if (!waiter.wait(timeoutMs)) {
 *     // handle timeout
 *   }
 */
class FenceWaiter final {
 public:
  FenceWaiter(ID3D12Fence* fence, UINT64 targetValue);
  ~FenceWaiter();

  // Delete copy/move to ensure single ownership of event handle
  FenceWaiter(const FenceWaiter&) = delete;
  FenceWaiter& operator=(const FenceWaiter&) = delete;
  FenceWaiter(FenceWaiter&&) = delete;
  FenceWaiter& operator=(FenceWaiter&&) = delete;

  /**
   * @brief Wait for fence to reach target value with timeout
   * @param timeoutMs Timeout in milliseconds (INFINITE for no timeout)
   * @return true if fence reached target, false on timeout or error
   */
  bool wait(DWORD timeoutMs = INFINITE);

  /**
   * @brief Check if fence already reached target without waiting
   */
  bool isComplete() const;

 private:
  ID3D12Fence* fence_;
  UINT64 targetValue_;
  HANDLE event_ = nullptr;
  bool setupSucceeded_ = false;
};

} // namespace igl::d3d12
