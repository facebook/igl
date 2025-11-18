/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>
#include <queue>
#include <mutex>

namespace igl::d3d12 {

/**
 * @brief Upload Ring Buffer for Streaming Resources (P1_DX12-009)
 *
 * Manages a large staging buffer (64-256MB) for efficient resource uploads.
 * Implements a ring buffer pattern with fence-based memory retirement to
 * reduce allocator churn and memory fragmentation.
 *
 * Key Features:
 * - Large pre-allocated upload heap
 * - Linear sub-allocation with wraparound
 * - Fence-based memory retirement and recycling
 * - Thread-safe allocation
 */
class UploadRingBuffer {
 public:
  /**
   * @brief Represents a sub-allocation from the ring buffer
   */
  struct Allocation {
    igl::d3d12::ComPtr<ID3D12Resource> buffer;  // T07: Underlying buffer resource
    void* cpuAddress = nullptr;                     // CPU-visible mapped address
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;      // GPU virtual address
    uint64_t offset = 0;                            // Offset within ring buffer
    uint64_t size = 0;                              // Size of allocation
    bool valid = false;                             // Whether allocation succeeded
  };

  /**
   * @brief Constructs an upload ring buffer
   * @param device D3D12 device for resource creation
   * @param size Total size of ring buffer (default: 128MB)
   *
   * T14: Default value (128MB) matches D3D12ContextConfig::defaultConfig().uploadRingBufferSize
   * Configurable by passing explicit size parameter at construction time.
   * TODO: Wire D3D12ContextConfig::uploadRingBufferSize into call sites for automatic configuration.
   */
  explicit UploadRingBuffer(ID3D12Device* device, uint64_t size = 128 * 1024 * 1024);
  ~UploadRingBuffer();

  // Non-copyable
  UploadRingBuffer(const UploadRingBuffer&) = delete;
  UploadRingBuffer& operator=(const UploadRingBuffer&) = delete;

  /**
   * @brief Allocates staging memory from the ring buffer
   * @param size Size in bytes to allocate
   * @param alignment Alignment requirement (e.g., 256 for constant buffers)
   * @param fenceValue Fence value when this allocation will be retired
   * @return Allocation structure (check valid flag for success)
   *
   * Note: If allocation fails due to insufficient space, returns invalid allocation.
   * Caller should fall back to creating a dedicated staging buffer.
   */
  Allocation allocate(uint64_t size, uint64_t alignment, uint64_t fenceValue);

  /**
   * @brief Retires allocations that have completed on GPU
   * @param completedFenceValue Fence value that has been signaled by GPU
   *
   * Reclaims memory from allocations associated with fence values <= completedFenceValue.
   * This allows the ring buffer to wrap around and reuse memory.
   */
  void retire(uint64_t completedFenceValue);

  /**
   * @brief Gets total size of ring buffer
   */
  uint64_t getTotalSize() const { return size_; }

  /**
   * @brief Gets estimated used size based on head/tail distance (for diagnostics)
   *
   * Note: Returns approximate usage; does not account for internal alignment gaps.
   * Returns 0 when buffer is empty (tail == head with no pending allocations).
   * Also returns 0 when buffer is completely full (tail == head with pending allocations);
   * use pendingAllocations or getFailureCount() to distinguish empty vs. full states.
   */
  uint64_t getUsedSize() const;

  /**
   * @brief Gets number of allocations made (for performance metrics)
   */
  uint64_t getAllocationCount() const { return allocationCount_; }

  /**
   * @brief Gets number of times allocation could not be satisfied from ring buffer (for metrics)
   *
   * Note: This counts ring-full events where callers fall back to dedicated staging buffers,
   * not error conditions. It is a diagnostic metric for ring buffer utilization.
   */
  uint64_t getFailureCount() const { return failureCount_; }

  /**
   * @brief Gets the underlying upload heap resource (for copy operations)
   */
  ID3D12Resource* getUploadHeap() const { return uploadHeap_.Get(); }

 private:
  /**
   * @brief Represents a pending allocation waiting for GPU completion
   */
  struct PendingAllocation {
    uint64_t offset;      // Start offset in ring buffer
    uint64_t size;        // Size of allocation
    uint64_t fenceValue;  // Fence value when allocation can be retired
  };

  /**
   * @brief Aligns value up to specified alignment
   */
  static uint64_t alignUp(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  /**
   * @brief Internal helper to compute used size without locking
   * @note Caller must hold mutex_
   * @note Returns 0 when head == tail (both empty and full states)
   */
  uint64_t getUsedSizeUnlocked() const;


  ID3D12Device* device_ = nullptr;
  igl::d3d12::ComPtr<ID3D12Resource> uploadHeap_;
  void* cpuBase_ = nullptr;                         // CPU-mapped base address
  D3D12_GPU_VIRTUAL_ADDRESS gpuBase_ = 0;          // GPU base address

  uint64_t size_ = 0;                               // Total ring buffer size
  uint64_t head_ = 0;                               // Next free offset for new allocations (protected by mutex_)
  uint64_t tail_ = 0;                               // Offset of oldest in-flight allocation; equals head_ when empty (protected by mutex_)

  std::queue<PendingAllocation> pendingAllocations_; // Allocations waiting for GPU
  mutable std::mutex mutex_;                        // Thread safety

  // Metrics
  uint64_t allocationCount_ = 0;
  uint64_t failureCount_ = 0;
};

} // namespace igl::d3d12
