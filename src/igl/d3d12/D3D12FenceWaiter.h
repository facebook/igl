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
 * IMPORTANT: The fence pointer must remain valid for the lifetime of the FenceWaiter.
 * Typical usage is with fences owned by long-lived context objects.
 *
 * Usage:
 *   FenceWaiter waiter(fence, targetValue);
 *   Result result = waiter.wait(timeoutMs);
 *   if (!result.isOk()) {
 *     // Handle specific error (timeout, setup failure, etc.)
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
   * @return Result with specific error code and message on failure:
   *         - ArgumentNull: Null fence provided to constructor
   *         - InvalidOperation: Event creation or SetEventOnCompletion failed
   *         - RuntimeError: Wait timed out (use isTimeoutError() to detect)
   *         - RuntimeError: Wait failed or fence incomplete after event signaled
   */
  Result wait(DWORD timeoutMs = INFINITE);

  /**
   * @brief Check if fence already reached target without waiting
   */
  bool isComplete() const;

  /**
   * @brief Check if a Result represents a timeout error
   * @param result The Result to check
   * @return true if the result indicates a timeout, false otherwise
   */
  static bool isTimeoutError(const Result& result);

 private:
  ID3D12Fence* fence_;
  UINT64 targetValue_;
  HANDLE event_ = nullptr;
  bool setupSucceeded_ = false;
  Result::Code setupErrorCode_ = Result::Code::Ok;
  std::string setupErrorMessage_;
};

} // namespace igl::d3d12
