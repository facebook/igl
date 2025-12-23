/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Timer.h>

#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/Device.h>

namespace igl::d3d12 {

Timer::Timer(const Device& device) {
  auto& ctx = device.getD3D12Context();
  auto* d3dDevice = ctx.getDevice();
  auto* commandQueue = ctx.getCommandQueue();

  // Query GPU timestamp frequency
  // This returns the number of ticks per second for GPU timestamps
  HRESULT hr = commandQueue->GetTimestampFrequency(&timestampFrequency_);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("Timer: Failed to get timestamp frequency (0x%08X). Timer disabled.\n", hr);
    resourceCreationFailed_ = true;
    timestampFrequency_ = 0; // Leave at 0 to indicate timer is disabled
    return;
  }

  // Create query heap for 2 timestamps (begin and end).
  // Use D3D12_QUERY_HEAP_TYPE_TIMESTAMP for GPU timer queries.
  D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
  queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
  queryHeapDesc.Count = 2; // Begin and end timestamps
  queryHeapDesc.NodeMask = 0; // Single GPU

  hr = d3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(queryHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("Timer: Failed to create query heap (0x%08X). Timer disabled.\n", hr);
    resourceCreationFailed_ = true;
    timestampFrequency_ = 0;
    return;
  }

  // Create readback buffer to hold query results
  // Must use READBACK heap type for CPU access
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resourceDesc.Alignment = 0;
  resourceDesc.Width = 2 * sizeof(uint64_t); // Space for 2 timestamps
  resourceDesc.Height = 1;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.SampleDesc.Quality = 0;
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  hr = d3dDevice->CreateCommittedResource(&heapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &resourceDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, // Readback buffers must
                                                                          // be in COPY_DEST state
                                          nullptr,
                                          IID_PPV_ARGS(readbackBuffer_.GetAddressOf()));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("Timer: Failed to create readback buffer (0x%08X). Timer disabled.\n", hr);
    resourceCreationFailed_ = true;
    timestampFrequency_ = 0;
    queryHeap_.Reset(); // Clean up partially created resources
    return;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("Timer: Created successfully (frequency: %llu Hz)\n", timestampFrequency_);
#endif
}

Timer::~Timer() {
  // ComPtr handles cleanup automatically
}

void Timer::begin(ID3D12GraphicsCommandList* commandList) {
  if (resourceCreationFailed_ || timestampFrequency_ == 0) {
    // Timer disabled due to resource creation or frequency query failure - silently no-op
    return;
  }

  if (!commandList) {
    IGL_LOG_ERROR("Timer::begin() called with null command list\n");
    return;
  }

  // Record begin timestamp (index 0) at the start of GPU work.
  // This is a bottom-of-pipe operation that samples when the GPU finishes preceding work.
  commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
}

void Timer::end(ID3D12GraphicsCommandList* commandList, ID3D12Fence* fence, uint64_t fenceValue) {
  if (resourceCreationFailed_ || timestampFrequency_ == 0) {
    // Timer disabled - silently no-op
    return;
  }

  if (!commandList) {
    IGL_LOG_ERROR("Timer::end() called with null command list\n");
    return;
  }

  if (!fence) {
    IGL_LOG_ERROR("Timer::end() called with null fence\n");
    return;
  }

  if (ended_.load(std::memory_order_acquire)) {
    IGL_LOG_ERROR("Timer::end() called multiple times\n");
    return;
  }

  // Record end timestamp (index 1) at the end of GPU work.
  // Bottom-of-pipe operation: samples when the GPU finishes all preceding work.
  commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

  // Resolve query data to the readback buffer.
  // This GPU command copies timestamp values from the query heap to a CPU-readable buffer;
  // the resolved data is only valid after the fence signals completion.
  commandList->ResolveQueryData(queryHeap_.Get(),
                                D3D12_QUERY_TYPE_TIMESTAMP,
                                0, // Start index
                                2, // Count (begin + end)
                                readbackBuffer_.Get(),
                                0 // Destination offset
  );

  // Store fence and fence value for later completion checking.
  // Thread-safe: fence_ is written once; atomics ensure visibility.
  fence_ = fence;
  fenceValue_.store(fenceValue, std::memory_order_release);
  ended_.store(true, std::memory_order_release);
}

uint64_t Timer::getElapsedTimeNanos() const {
  if (!readbackBuffer_.Get() || !ended_.load(std::memory_order_acquire)) {
    return 0;
  }

  // Check if the fence has signaled; results are only valid after GPU completes.
  // Thread-safe: fence_ is set once before the ended_ flag, and memory ordering ensures visibility.
  uint64_t fenceVal = fenceValue_.load(std::memory_order_acquire);
  if (!fence_ || fence_->GetCompletedValue() < fenceVal) {
    return 0; // GPU hasn't finished yet, return 0
  }

  // If we've already resolved and cached the result, return it.
  // Thread-safe: resolved_ flag prevents multiple threads from mapping simultaneously.
  if (resolved_.load(std::memory_order_acquire)) {
    return cachedElapsedNanos_.load(std::memory_order_relaxed);
  }

  // GPU has completed; it is now safe to read the query results.
  // Map the readback buffer to read timestamp values.
  void* mappedData = nullptr;
  D3D12_RANGE readRange{0, sizeof(uint64_t) * 2}; // Only read the 2 timestamps
  HRESULT hr = readbackBuffer_->Map(0, &readRange, &mappedData);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("Timer: Failed to map readback buffer: 0x%08X\n", hr);
    return 0;
  }

  // Read timestamp values
  const auto* timestamps = static_cast<const uint64_t*>(mappedData);
  uint64_t beginTime = timestamps[0];
  uint64_t endTime = timestamps[1];

  // Unmap buffer
  D3D12_RANGE writeRange{0, 0}; // No writes
  readbackBuffer_->Unmap(0, &writeRange);

  // Validate timestamp data
  if (endTime <= beginTime) {
#ifdef IGL_DEBUG
    IGL_LOG_ERROR(
        "Timer: Invalid timestamp data (begin=%llu, end=%llu) - GPU work may not have executed\n",
        beginTime,
        endTime);
#endif
    return 0;
  }

  if (timestampFrequency_ == 0) {
#ifdef IGL_DEBUG
    IGL_LOG_ERROR("Timer: Invalid timestamp frequency (0 Hz) - timer disabled\n");
#endif
    return 0;
  }

  // Calculate elapsed time in GPU ticks
  uint64_t deltaTicks = endTime - beginTime;

  // Convert ticks to nanoseconds using floating-point math for accuracy,
  // as recommended by Microsoft docs: nanoseconds = (ticks / frequency) * 1,000,000,000.
  const double nanosPerSecond = 1000000000.0;
  double elapsedNanos =
      (static_cast<double>(deltaTicks) / static_cast<double>(timestampFrequency_)) * nanosPerSecond;

  // Cache the result so we don't re-read from GPU.
  // Thread-safe: store cached value before setting the resolved flag.
  cachedElapsedNanos_.store(static_cast<uint64_t>(elapsedNanos), std::memory_order_release);
  resolved_.store(true, std::memory_order_release);

  return static_cast<uint64_t>(elapsedNanos);
}

bool Timer::resultsAvailable() const {
  // Results are available only after the fence has signaled completion.
  // This ensures we don't read uninitialized or garbage data from the query heap.
  // Thread-safe: use atomic loads with proper memory ordering.
  if (!ended_.load(std::memory_order_acquire) || !fence_) {
    return false;
  }

  // Check if GPU has completed execution (fence signaled)
  uint64_t fenceVal = fenceValue_.load(std::memory_order_acquire);
  return fence_->GetCompletedValue() >= fenceVal;
}

} // namespace igl::d3d12
