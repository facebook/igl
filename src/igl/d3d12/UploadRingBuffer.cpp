/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/UploadRingBuffer.h>
#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

UploadRingBuffer::UploadRingBuffer(ID3D12Device* device, uint64_t size)
    : device_(device), size_(size) {
  if (!device_) {
    IGL_LOG_ERROR("UploadRingBuffer: Device is null\n");
    return;
  }

  // Create large upload heap
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  uploadHeapProps.CreationNodeMask = 1;
  uploadHeapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Alignment = 0;
  bufferDesc.Width = size_;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.SampleDesc.Quality = 0;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  HRESULT hr = device_->CreateCommittedResource(
      &uploadHeapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadHeap_.GetAddressOf()));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("UploadRingBuffer: Failed to create upload heap (HRESULT=0x%08X)\n", hr);
    return;
  }

  // Map the entire buffer persistently
  D3D12_RANGE readRange = {0, 0}; // Not reading from GPU
  hr = uploadHeap_->Map(0, &readRange, &cpuBase_);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("UploadRingBuffer: Failed to map upload heap (HRESULT=0x%08X)\n", hr);
    cpuBase_ = nullptr;
    return;
  }

  gpuBase_ = uploadHeap_->GetGPUVirtualAddress();

  IGL_D3D12_LOG_VERBOSE("UploadRingBuffer: Created ring buffer (size=%llu MB, cpuBase=%p, gpuBase=0x%llX)\n",
               size_ / (1024 * 1024), cpuBase_, gpuBase_);

  // Track resource creation
  D3D12Context::trackResourceCreation("UploadRingBuffer", size_);
}

UploadRingBuffer::~UploadRingBuffer() {
  if (uploadHeap_.Get() && cpuBase_) {
    uploadHeap_->Unmap(0, nullptr);
    cpuBase_ = nullptr;
  }

  if (uploadHeap_.Get()) {
    // Track resource destruction
    D3D12Context::trackResourceDestruction("UploadRingBuffer", size_);
  }

  IGL_D3D12_LOG_VERBOSE("UploadRingBuffer: Destroyed (allocations=%llu, failures=%llu)\n",
               allocationCount_, failureCount_);
}

UploadRingBuffer::Allocation UploadRingBuffer::allocate(uint64_t size, uint64_t alignment, uint64_t fenceValue) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!uploadHeap_.Get() || !cpuBase_) {
    IGL_LOG_ERROR("UploadRingBuffer::allocate: Ring buffer not initialized\n");
    failureCount_++;
    return Allocation{};
  }

  if (size == 0) {
    IGL_LOG_ERROR("UploadRingBuffer::allocate: Size is zero\n");
    failureCount_++;
    return Allocation{};
  }

  // Align size up for proper alignment of next allocation
  const uint64_t alignedSize = alignUp(size, alignment);

  // Load current head and tail atomically
  const uint64_t currentHead = head_.load(std::memory_order_acquire);
  const uint64_t currentTail = tail_.load(std::memory_order_acquire);

  // Align head to requested alignment
  const uint64_t alignedHead = alignUp(currentHead, alignment);

  // Check if we have enough contiguous space
  // Case 1: head <= tail (normal case, free space is at end and beginning)
  // Case 2: head > tail (wrapped, free space is between head and tail)

  bool canFit = false;
  uint64_t allocationOffset = alignedHead;

  if (currentHead <= currentTail) {
    // Free space: [head, size) and [0, tail)
    // Try to allocate at alignedHead first
    if (alignedHead + alignedSize <= size_) {
      canFit = true;
      allocationOffset = alignedHead;
    } else if (alignedSize <= currentTail) {
      // Wrap around to beginning - but validate tail hasn't moved
      allocationOffset = 0;

      // CRITICAL: Re-check tail after deciding to wrap
      // (retire() could have advanced it while we calculated)
      const uint64_t recheckTail = tail_.load(std::memory_order_acquire);
      if (alignedSize <= recheckTail) {
        canFit = true;
      } else {
        // Tail advanced during calculation, allocation would overlap!
        IGL_D3D12_LOG_VERBOSE("UploadRingBuffer: Wraparound race avoided "
                     "(tail advanced from %llu to %llu during allocation)\n", currentTail, recheckTail);
        canFit = false;
      }
    }
  } else {
    // Free space: [head, tail)
    if (alignedHead + alignedSize <= currentTail) {
      canFit = true;
      allocationOffset = alignedHead;
    }
  }

  if (!canFit) {
    // Not enough space - caller should fall back to dedicated staging buffer
    failureCount_++;
    return Allocation{};
  }

  // Validate allocation doesn't overlap with tail (final safety check)
  const uint64_t allocationEnd = allocationOffset + alignedSize;
  const uint64_t finalTail = tail_.load(std::memory_order_acquire);

  // CRITICAL FIX: When buffer is empty (no pending allocations), tail_ is meaningless
  // In this case, the entire buffer is free and we should allow the allocation
  const bool bufferEmpty = pendingAllocations_.empty();

  if (!bufferEmpty) {
    if (allocationOffset == 0) {
      // Wraparound case: ensure we don't exceed tail
      if (allocationEnd > finalTail) {
        IGL_LOG_ERROR("UploadRingBuffer: Allocation [0, %llu) would overlap tail at %llu\n",
                      allocationEnd, finalTail);
        failureCount_++;
        return Allocation{};
      }
    } else if (allocationEnd > size_) {
      // Allocation would wrap past buffer end - check for overlap with tail
      const uint64_t wrappedEnd = allocationEnd - size_;
      if (wrappedEnd > finalTail) {
        IGL_LOG_ERROR("UploadRingBuffer: Allocation would wrap and overlap tail (end=%llu, tail=%llu)\n",
                      wrappedEnd, finalTail);
        failureCount_++;
        return Allocation{};
      }
    }
  }

  // Create allocation
  Allocation allocation;
  allocation.buffer = uploadHeap_;  // T07: Set underlying buffer
  allocation.cpuAddress = static_cast<uint8_t*>(cpuBase_) + allocationOffset;
  allocation.gpuAddress = gpuBase_ + allocationOffset;
  allocation.offset = allocationOffset;
  allocation.size = alignedSize;
  allocation.valid = true;

  // Track pending allocation for retirement
  pendingAllocations_.push({allocationOffset, alignedSize, fenceValue});

  // Update head pointer atomically
  uint64_t newHead = allocationOffset + alignedSize;
  if (newHead >= size_) {
    newHead = 0; // Wrap around
  }
  head_.store(newHead, std::memory_order_release);

  allocationCount_++;

#ifdef _DEBUG
  // Debug validation: ensure invariants hold
  IGL_DEBUG_ASSERT(newHead <= size_, "Head exceeded buffer size!");

  // Validate free space calculation (skip if buffer was empty before this allocation)
  if (!bufferEmpty) {
    const uint64_t debugTail = tail_.load(std::memory_order_acquire);
    uint64_t freeSpace = (newHead <= debugTail)
        ? (size_ - newHead) + debugTail
        : debugTail - newHead;
    IGL_DEBUG_ASSERT(freeSpace <= size_, "Free space calculation invalid!");

    // Ensure this allocation doesn't overlap with any pending allocations
    // Note: This is a simplified check since we only track head/tail, not individual allocations
    if (allocationOffset < newHead) {
      // Normal case: allocation is [allocationOffset, newHead)
      // Check it doesn't overlap with tail region
      if (newHead > debugTail && allocationOffset < debugTail) {
        // Would wrap around and potentially overlap
        IGL_DEBUG_ASSERT(newHead <= size_, "Allocation wraps incorrectly!");
      }
    }
  }
#endif

  return allocation;
}

void UploadRingBuffer::retire(uint64_t completedFenceValue) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Process all pending allocations that have completed
  while (!pendingAllocations_.empty()) {
    const auto& pending = pendingAllocations_.front();

    if (pending.fenceValue > completedFenceValue) {
      // This and all subsequent allocations are still pending
      break;
    }

    // This allocation has completed, reclaim the memory
    uint64_t newTail = pending.offset + pending.size;
    if (newTail >= size_) {
      newTail = 0; // Wrap around
    }
    tail_.store(newTail, std::memory_order_release);

    pendingAllocations_.pop();
  }
}

uint64_t UploadRingBuffer::getUsedSize() const {
  std::lock_guard<std::mutex> lock(mutex_);

  const uint64_t currentHead = head_.load(std::memory_order_acquire);
  const uint64_t currentTail = tail_.load(std::memory_order_acquire);

  if (currentHead >= currentTail) {
    return currentHead - currentTail;
  } else {
    return (size_ - currentTail) + currentHead;
  }
}

bool UploadRingBuffer::canAllocate(uint64_t size) const {
  const uint64_t currentHead = head_.load(std::memory_order_acquire);
  const uint64_t currentTail = tail_.load(std::memory_order_acquire);

  if (currentHead <= currentTail) {
    // Free space at end and beginning
    return (size <= size_ - currentHead) || (size <= currentTail);
  } else {
    // Free space between head and tail
    return size <= currentTail - currentHead;
  }
}

} // namespace igl::d3d12
