/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <igl/Timer.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Device;

/// @brief GPU timer implementation using D3D12 timestamp queries
/// @details Implements ITimer interface for D3D12 backend using query heaps.
///
/// Lifecycle:
/// - Constructor creates query heap and readback buffer resources
/// - begin() called when command list is reset for recording (CommandBuffer::begin())
/// - GPU work is encoded in the command list
/// - end() called during submission before command list is closed (CommandQueue::submit())
/// - Query results are fence-synchronized and only read after GPU completes
///
/// Cross-platform timestamp semantics
/// ----------------------------------
/// All timestamps returned by getElapsedTimeNanos() are in nanoseconds, providing
/// cross-platform consistency with Vulkan and other backends.
///
/// D3D12 GPU timestamps are automatically converted from hardware ticks to nanoseconds
/// using the GPU timestamp frequency (ID3D12CommandQueue::GetTimestampFrequency()).
///
/// Formula: elapsedNanos = (endTicks - startTicks) * 1,000,000,000 / frequencyHz.
///
/// This ensures consistent timing across all IGL backends regardless of hardware.
///
/// The implementation measures GPU execution time via timestamp placement
/// and fence-synchronized readback, and is safe for cross-thread queries.
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

  /// @brief Record start timestamp in command list
  /// @param commandList D3D12 command list to record start timestamp
  void begin(ID3D12GraphicsCommandList* commandList);

  /// @brief Record end timestamp and associate with fence value
  /// @param commandList D3D12 command list to record end timestamp and resolve queries
  /// @param fence Fence to check for GPU completion
  /// @param fenceValue Fence value that will be signaled when GPU completes
  void end(ID3D12GraphicsCommandList* commandList, ID3D12Fence* fence, uint64_t fenceValue);

  /// @brief Returns elapsed GPU time in nanoseconds
  /// @return Elapsed time in nanoseconds, or 0 if results not yet available
  [[nodiscard]] uint64_t getElapsedTimeNanos() const override;

  /// @brief Check if timer results are available
  /// @return true if results can be read without blocking (fence has signaled)
  [[nodiscard]] bool resultsAvailable() const override;

 private:
  igl::d3d12::ComPtr<ID3D12QueryHeap> queryHeap_;
  igl::d3d12::ComPtr<ID3D12Resource> readbackBuffer_;
  uint64_t timestampFrequency_ =
      0; // GPU timestamp frequency (ticks per second), 0 = timer disabled
  bool resourceCreationFailed_ = false; // Track if constructor failed to create resources

  // Fence synchronization for accurate GPU timing.
  // Thread-safe: use atomics to allow safe cross-thread queries.
  ID3D12Fence* fence_ = nullptr; // Fence to check completion (not owned, set once in end())
  std::atomic<uint64_t> fenceValue_{0}; // Fence value when timer ended
  mutable std::atomic<bool> resolved_{false}; // Has query data been resolved and cached? (mutable
                                              // for lazy resolution in const getter)
  std::atomic<bool> ended_{false}; // Has end() been called?

  // Cached results to avoid re-reading from GPU.
  // Thread-safe: only written once after the fence signals, then immutable (mutable for lazy
  // resolution in const getter).
  mutable std::atomic<uint64_t> cachedElapsedNanos_{0};
};

} // namespace igl::d3d12
