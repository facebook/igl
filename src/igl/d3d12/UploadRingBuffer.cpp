/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/UploadRingBuffer.h>

#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

UploadRingBuffer::UploadRingBuffer(ID3D12Device* device, uint64_t size) :
  device_(device), size_(size) {
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

  HRESULT hr = device_->CreateCommittedResource(&uploadHeapProps,
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

  IGL_D3D12_LOG_VERBOSE(
      "UploadRingBuffer: Created ring buffer (size=%llu MB, cpuBase=%p, gpuBase=0x%llX)\n",
      size_ / (1024 * 1024),
      cpuBase_,
      gpuBase_);

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
                        allocationCount_,
                        failureCount_);
}

UploadRingBuffer::Allocation UploadRingBuffer::allocate(uint64_t size,
                                                        uint64_t alignment,
                                                        uint64_t fenceValue) {
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

  // Invariants (all protected by mutex_):
  // - head_ is the next free offset where a new allocation can start
  // - tail_ is the offset of the oldest in-flight allocation (or equals head_ when empty)
  // - pendingAllocations_ is a queue of all in-flight allocations in submission order
  // - When pendingAllocations_.empty(), the entire buffer is free: tail_ == head_
  const bool bufferEmpty = pendingAllocations_.empty();
  const uint64_t currentHead = head_;
  const uint64_t currentTail = bufferEmpty ? currentHead : tail_;

  // Detect full ring: head == tail with in-flight allocations means buffer is completely occupied
  const bool bufferFull = !bufferEmpty && (currentHead == currentTail);

  if (bufferFull) {
    // Ring buffer is completely full - no free space available
    failureCount_++;
    IGL_D3D12_LOG_VERBOSE("UploadRingBuffer: Ring buffer completely full (size=%llu)\n", size_);
    return Allocation{};
  }

  // Align head to requested alignment
  const uint64_t alignedHead = alignUp(currentHead, alignment);

  // Determine available free space based on buffer state
  // When empty: entire buffer is available starting from head_
  // When head > tail: in-flight region spans [tail, head); free space is [head, size_) and [0,
  // tail) When head < tail: in-flight region spans [tail, size_) + [0, head); free space is [head,
  // tail)

  bool canFit = false;
  uint64_t allocationOffset = alignedHead;

  if (bufferEmpty) {
    // Entire buffer is free
    if (alignedHead + alignedSize <= size_) {
      canFit = true;
      allocationOffset = alignedHead;
    } else if (alignedSize <= size_) {
      // Wrap to beginning
      allocationOffset = 0;
      canFit = true;
    }
  } else if (currentHead >= currentTail) {
    // In-flight allocations have wrapped around: free regions are [head, size_) and [0, tail)
    if (alignedHead + alignedSize <= size_) {
      // Fits at current head position
      canFit = true;
      allocationOffset = alignedHead;
    } else if (alignedSize <= currentTail) {
      // Wrap around to beginning
      allocationOffset = 0;
      canFit = true;
    }
  } else {
    // In-flight allocations have not wrapped: free space is [head, tail)
    if (alignedHead + alignedSize <= currentTail) {
      canFit = true;
      allocationOffset = alignedHead;
    }
  }

  if (!canFit) {
    // Not enough space - caller will fall back to dedicated staging buffer
    // This is expected behavior when ring is full, not an error condition
    // Note: failureCount_ tracks ring-full events as a diagnostic metric, not errors
    failureCount_++;
    IGL_D3D12_LOG_VERBOSE(
        "UploadRingBuffer: Insufficient space (request=%llu, approx used=%llu/%llu)\n",
        alignedSize,
        getUsedSizeUnlocked(),
        size_);
    return Allocation{};
  }

  // Final validation: ensure allocation doesn't overlap with in-flight allocations
  const uint64_t allocationEnd = allocationOffset + alignedSize;

#ifdef _DEBUG
  // Debug: verify allocation doesn't overlap with in-flight allocations
  if (!bufferEmpty) {
    if (allocationOffset == 0) {
      // Wraparound case: ensure we don't exceed tail
      IGL_DEBUG_ASSERT(allocationEnd <= currentTail,
                       "UploadRingBuffer: Allocation [0, %llu) would overlap tail at %llu",
                       allocationEnd,
                       currentTail);
    } else if (currentHead >= currentTail) {
      // In-flight region wrapped: allocation should be in free region [head, size_)
      IGL_DEBUG_ASSERT(allocationOffset >= currentHead && allocationEnd <= size_,
                       "UploadRingBuffer: Allocation [%llu, %llu) outside free region [%llu, %llu)",
                       allocationOffset,
                       allocationEnd,
                       currentHead,
                       size_);
    } else {
      // In-flight region not wrapped: allocation should be in free region [head, tail)
      IGL_DEBUG_ASSERT(allocationOffset >= currentHead && allocationEnd <= currentTail,
                       "UploadRingBuffer: Allocation [%llu, %llu) outside free region [%llu, %llu)",
                       allocationOffset,
                       allocationEnd,
                       currentHead,
                       currentTail);
    }
  }
#endif

  // Create allocation.
  Allocation allocation;
  allocation.buffer = uploadHeap_;
  allocation.cpuAddress = static_cast<uint8_t*>(cpuBase_) + allocationOffset;
  allocation.gpuAddress = gpuBase_ + allocationOffset;
  allocation.offset = allocationOffset;
  allocation.size = alignedSize;
  allocation.valid = true;

  // Track pending allocation for retirement
  pendingAllocations_.push({allocationOffset, alignedSize, fenceValue});

  // Update head pointer
  uint64_t newHead = allocationOffset + alignedSize;
  if (newHead >= size_) {
    newHead = 0; // Wrap around
  }
  head_ = newHead;

  // Update tail_ for first allocation when buffer transitions from empty
  if (bufferEmpty) {
    tail_ = allocationOffset;
  }

  allocationCount_++;

#ifdef _DEBUG
  // Debug validation: ensure invariants hold after allocation
  IGL_DEBUG_ASSERT(newHead <= size_, "Head exceeded buffer size!");
  IGL_DEBUG_ASSERT(!pendingAllocations_.empty() || head_ == tail_,
                   "Buffer should have pending allocations or head == tail");

  // Validate that used size is reasonable (use unlocked helper since we hold mutex_)
  const uint64_t usedSize = getUsedSizeUnlocked();
  IGL_DEBUG_ASSERT(usedSize <= size_, "Used size %llu exceeds buffer size %llu", usedSize, size_);
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
    pendingAllocations_.pop();

    // Update tail_ to point to the next oldest allocation, or to head_ if buffer is now empty
    if (!pendingAllocations_.empty()) {
      tail_ = pendingAllocations_.front().offset;
    } else {
      // Buffer is now empty: reset tail to head to maintain invariant
      tail_ = head_;
    }
  }

#ifdef _DEBUG
  // Validate invariant: when empty, tail == head
  if (pendingAllocations_.empty()) {
    IGL_DEBUG_ASSERT(tail_ == head_, "Buffer empty but tail (%llu) != head (%llu)", tail_, head_);
  }
#endif
}

uint64_t UploadRingBuffer::getUsedSizeUnlocked() const {
  // Note: Caller must hold mutex_
  if (head_ >= tail_) {
    return head_ - tail_;
  } else {
    return (size_ - tail_) + head_;
  }
}

uint64_t UploadRingBuffer::getUsedSize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return getUsedSizeUnlocked();
}

} // namespace igl::d3d12
