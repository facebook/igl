/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>
#include <vector>
#include <mutex>

namespace igl::d3d12 {

/**
 * @brief Interface for obtaining fence values from shared timeline
 *
 * This interface allows D3D12ImmediateCommands to participate in the
 * device's shared fence timeline without managing its own counter.
 */
class IFenceProvider {
public:
  virtual ~IFenceProvider() = default;

  /**
   * @brief Get the next fence value from the shared timeline
   * @return Monotonically increasing fence value
   */
  virtual uint64_t getNextFenceValue() = 0;
};

/**
 * @brief Centralized management of immediate copy operations
 *
 * Provides a pooled command allocator/list infrastructure for transient
 * upload/readback operations, eliminating per-operation allocator creation
 * and redundant GPU synchronization.
 *
 * Thread-safety: This class is NOT thread-safe for concurrent begin()/submit().
 * Only one begin()/submit() sequence may be active at a time. Multiple threads
 * calling begin() concurrently will corrupt the shared command list.
 *
 * The allocator pool (reclaimCompletedAllocators) is internally synchronized.
 *
 * Inspired by Vulkan's VulkanImmediateCommands pattern.
 */
class D3D12ImmediateCommands {
public:
  /**
   * @brief Initialize the immediate commands infrastructure
   * @param device D3D12 device for resource creation
   * @param queue Command queue for submission
   * @param fence Fence for completion tracking (shared with device)
   * @param fenceProvider Provider for next fence values from shared timeline
   */
  D3D12ImmediateCommands(ID3D12Device* device,
                         ID3D12CommandQueue* queue,
                         ID3D12Fence* fence,
                         IFenceProvider* fenceProvider);

  ~D3D12ImmediateCommands();

  /**
   * @brief Get command list for immediate copy operation
   *
   * Returns a ready-to-use command list from the pool. The command list
   * is already reset and ready for recording.
   *
   * @param outResult Optional result for error reporting
   * @return Command list ready for recording, or nullptr on failure
   */
  [[nodiscard]] ID3D12GraphicsCommandList* begin(Result* outResult = nullptr);

  /**
   * @brief Submit command list and optionally wait for completion
   *
   * Closes, submits, and signals the fence. If wait=true, blocks until
   * GPU completes the work.
   *
   * @param wait If true, block until GPU completes
   * @param outResult Optional result for error reporting
   * @return Fence value that will signal when work completes (0 on failure)
   */
  [[nodiscard]] uint64_t submit(bool wait, Result* outResult = nullptr);

  /**
   * @brief Check if a fence value has completed
   * @param fenceValue Fence value to check
   * @return true if GPU has completed this fence value
   */
  [[nodiscard]] bool isComplete(uint64_t fenceValue) const;

  /**
   * @brief Wait for a specific fence value to complete
   * @param fenceValue Fence value to wait for
   * @return Result indicating success or failure
   */
  [[nodiscard]] Result waitForFence(uint64_t fenceValue);

private:
  /**
   * @brief Reclaim completed command allocators back to pool
   *
   * Internal method called during begin() to recycle allocators.
   * Must be called with poolMutex_ held.
   */
  void reclaimCompletedAllocators();
  struct AllocatorEntry {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    uint64_t fenceValue = 0;  // Fence value when this allocator was last used
  };

  ID3D12Device* device_ = nullptr;
  ID3D12CommandQueue* queue_ = nullptr;
  ID3D12Fence* fence_ = nullptr;  // Shared fence (owned by Device)
  IFenceProvider* fenceProvider_ = nullptr;  // Provides fence values from shared timeline

  // Current command list for recording
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList_;

  // Current allocator being used
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> currentAllocator_;

  // Pool of available allocators
  std::vector<AllocatorEntry> availableAllocators_;

  // Allocators in flight (waiting for GPU)
  std::vector<AllocatorEntry> inFlightAllocators_;

  // Mutex for thread-safe allocator pool access
  std::mutex poolMutex_;

  // Get or create an allocator from the pool
  [[nodiscard]] Result getOrCreateAllocator(
      Microsoft::WRL::ComPtr<ID3D12CommandAllocator>* outAllocator);
};

} // namespace igl::d3d12
