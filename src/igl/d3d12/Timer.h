/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Timer.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Device;

/// @brief GPU timer implementation using D3D12 timestamp queries
/// @details Implements ITimer interface for D3D12 backend using query heaps.
/// Timer starts recording immediately on construction and ends when command buffer is submitted.
/// TASK_P2_DX12-FIND-11: Implement GPU Timer Queries
class Timer final : public ITimer {
 public:
  /// @brief Constructor - creates query heap and readback buffer, starts timer
  /// @param device D3D12 device used to create resources
  explicit Timer(const Device& device);
  ~Timer() override;

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;
  Timer(Timer&&) = delete;
  Timer& operator=(Timer&&) = delete;

  /// @brief Called by CommandQueue when command buffer is submitted
  /// @param commandList D3D12 command list to record timestamp and resolve queries
  void end(ID3D12GraphicsCommandList* commandList);

  /// @brief Returns elapsed GPU time in nanoseconds
  /// @return Elapsed time in nanoseconds, or 0 if results not yet available
  [[nodiscard]] uint64_t getElapsedTimeNanos() const override;

  /// @brief Check if timer results are available
  /// @return true if results can be read without blocking
  [[nodiscard]] bool resultsAvailable() const override;

 private:
  Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
  Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
  uint64_t timestampFrequency_ = 0;  // GPU timestamp frequency (ticks per second)
  bool ended_ = false;  // Track if end() has been called
};

} // namespace igl::d3d12
