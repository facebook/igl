/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mutex>
#include <vector>
#include <igl/Common.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>

namespace igl::d3d12 {

class UploadRingBuffer;

/**
 * @brief Centralized management of staging buffers for upload/readback
 *
 * Provides pooled staging buffer allocation for upload and readback operations,
 * eliminating per-operation staging buffer creation and improving reuse.
 *
 * Inspired by Vulkan's VulkanStagingDevice pattern.
 */
class D3D12StagingDevice {
 public:
  /**
   * @brief Staging buffer allocation
   */
  struct StagingBuffer {
    igl::d3d12::ComPtr<ID3D12Resource> buffer;
    void* mappedPtr = nullptr;
    size_t size = 0;
    uint64_t offset = 0; // Offset within buffer (for ring buffer allocations)
    bool valid = false;
    bool isFromRingBuffer = false; // True if allocated from ring buffer

    StagingBuffer() = default;
  };

  /**
   * @brief Initialize the staging device
   * @param device D3D12 device for resource creation
   * @param fence Fence for completion tracking
   * @param uploadRingBuffer Optional existing upload ring buffer to integrate
   */
  D3D12StagingDevice(ID3D12Device* device,
                     ID3D12Fence* fence,
                     UploadRingBuffer* uploadRingBuffer = nullptr);

  ~D3D12StagingDevice();

  /**
   * @brief Allocate a staging buffer for upload operations
   *
   * First attempts to use the upload ring buffer if available and size permits.
   * Falls back to creating a dedicated staging buffer for large allocations.
   *
   * @param size Size in bytes
   * @param alignment Required alignment (default 256 for constant buffers)
   * @param fenceValue Fence value when this allocation will be used
   * @return Staging buffer allocation
   */
  [[nodiscard]] StagingBuffer allocateUpload(size_t size,
                                             size_t alignment = 256,
                                             uint64_t fenceValue = 0);

  /**
   * @brief Allocate a staging buffer for readback operations
   *
   * Readback buffers are in READBACK heap (CPU-readable after GPU write).
   *
   * @param size Size in bytes
   * @return Staging buffer allocation
   */
  [[nodiscard]] StagingBuffer allocateReadback(size_t size);

  /**
   * @brief Free a staging buffer
   *
   * Buffers allocated from ring buffer are automatically recycled.
   * Dedicated buffers are pooled for reuse.
   *
   * @param buffer Buffer to free
   * @param fenceValue Fence value when GPU is done using this buffer
   */
  void free(StagingBuffer buffer, uint64_t fenceValue);

 private:
  /**
   * @brief Reclaim completed staging buffers back to pool
   *
   * Internal method called during allocate* to recycle buffers.
   * Must be called with poolMutex_ held.
   */
  void reclaimCompletedBuffers();
  struct BufferEntry {
    igl::d3d12::ComPtr<ID3D12Resource> buffer;
    size_t size = 0;
    uint64_t fenceValue = 0; // Fence value when this buffer was last used
    bool isReadback = false; // True for READBACK heap, false for UPLOAD heap
  };

  ID3D12Device* device_ = nullptr;
  ID3D12Fence* fence_ = nullptr;
  UploadRingBuffer* uploadRingBuffer_ = nullptr;

  // Pool of available staging buffers
  std::vector<BufferEntry> availableBuffers_;

  // Buffers in flight (waiting for GPU)
  std::vector<BufferEntry> inFlightBuffers_;

  // Mutex for thread-safe pool access
  std::mutex poolMutex_;

  // Maximum size to use ring buffer (larger allocations get dedicated buffers)
  static constexpr size_t kMaxRingBufferAllocation = 1024 * 1024; // 1MB

  // Create a new staging buffer
  [[nodiscard]] Result createStagingBuffer(size_t size,
                                           bool forReadback,
                                           igl::d3d12::ComPtr<ID3D12Resource>* outBuffer);

  // Find a reusable buffer from the pool
  [[nodiscard]] bool findReusableBuffer(size_t size,
                                        bool forReadback,
                                        igl::d3d12::ComPtr<ID3D12Resource>* outBuffer);
};

} // namespace igl::d3d12
