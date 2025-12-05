/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12StagingDevice.h>
#include <igl/d3d12/D3D12FenceWaiter.h>
#include <igl/d3d12/UploadRingBuffer.h>
#include <igl/d3d12/Common.h>
#include <igl/Assert.h>

namespace igl::d3d12 {

D3D12StagingDevice::D3D12StagingDevice(ID3D12Device* device,
                                       ID3D12Fence* fence,
                                       UploadRingBuffer* uploadRingBuffer)
    : device_(device), fence_(fence), uploadRingBuffer_(uploadRingBuffer) {
  IGL_DEBUG_ASSERT(device_);
  IGL_DEBUG_ASSERT(fence_);

  IGL_D3D12_LOG_VERBOSE("D3D12StagingDevice: Initialized (ring buffer: %s)\n",
               uploadRingBuffer_ ? "yes" : "no");
}

D3D12StagingDevice::~D3D12StagingDevice() {
  // Wait for all in-flight buffers to complete
  if (fence_) {
    for (const auto& entry : inFlightBuffers_) {
      if (fence_->GetCompletedValue() < entry.fenceValue) {
        FenceWaiter waiter(fence_, entry.fenceValue);
        Result waitResult = waiter.wait();
        if (!waitResult.isOk()) {
          IGL_LOG_ERROR("D3D12StagingDevice::~D3D12StagingDevice() - Fence wait failed during cleanup: %s\n",
                        waitResult.message.c_str());
        }
      }
    }
  }

  IGL_D3D12_LOG_VERBOSE("D3D12StagingDevice: Destroyed\n");
}

D3D12StagingDevice::StagingBuffer D3D12StagingDevice::allocateUpload(size_t size,
                                                                      size_t alignment,
                                                                      uint64_t fenceValue) {
  // Try ring buffer first for small allocations
  if (uploadRingBuffer_ && size <= kMaxRingBufferAllocation) {
    auto ringAlloc = uploadRingBuffer_->allocate(size, alignment, fenceValue);
    if (ringAlloc.valid) {
      StagingBuffer result;
      result.buffer = ringAlloc.buffer;
      result.mappedPtr = ringAlloc.cpuAddress;
      result.size = ringAlloc.size;
      result.offset = ringAlloc.offset;
      result.valid = true;
      result.isFromRingBuffer = true;
      return result;
    }
  }

  // Fall back to dedicated staging buffer
  std::lock_guard<std::mutex> lock(poolMutex_);

  // Reclaim completed buffers
  reclaimCompletedBuffers();

  igl::d3d12::ComPtr<ID3D12Resource> buffer;

  // Try to find a reusable buffer
  if (!findReusableBuffer(size, false, &buffer)) {
    // Create new buffer
    Result result = createStagingBuffer(size, false, &buffer);
    if (!result.isOk() || !buffer.Get()) {
      return StagingBuffer{};  // Return invalid buffer
    }
  }

  // Map the buffer
  void* mappedPtr = nullptr;
  D3D12_RANGE readRange{0, 0};  // Not reading
  HRESULT hr = buffer->Map(0, &readRange, &mappedPtr);
  if (FAILED(hr) || !mappedPtr) {
    IGL_LOG_ERROR("D3D12StagingDevice: Failed to map upload buffer\n");
    return StagingBuffer{};
  }

  StagingBuffer staging;
  staging.buffer = buffer;
  staging.mappedPtr = mappedPtr;
  staging.size = size;
  staging.offset = 0;
  staging.valid = true;
  staging.isFromRingBuffer = false;

  return staging;
}

D3D12StagingDevice::StagingBuffer D3D12StagingDevice::allocateReadback(size_t size) {
  std::lock_guard<std::mutex> lock(poolMutex_);

  // Reclaim completed buffers
  reclaimCompletedBuffers();

  igl::d3d12::ComPtr<ID3D12Resource> buffer;

  // Try to find a reusable buffer
  if (!findReusableBuffer(size, true, &buffer)) {
    // Create new buffer
    Result result = createStagingBuffer(size, true, &buffer);
    if (!result.isOk() || !buffer.Get()) {
      return StagingBuffer{};  // Return invalid buffer
    }
  }

  // Readback buffers are mapped on-demand when needed
  StagingBuffer staging;
  staging.buffer = buffer;
  staging.mappedPtr = nullptr;
  staging.size = size;
  staging.offset = 0;
  staging.valid = true;
  staging.isFromRingBuffer = false;

  return staging;
}

void D3D12StagingDevice::free(StagingBuffer buffer, uint64_t fenceValue) {
  if (!buffer.valid) {
    return;
  }

  // Ring buffer allocations are handled automatically
  if (buffer.isFromRingBuffer) {
    return;
  }

  std::lock_guard<std::mutex> lock(poolMutex_);

  // Unmap if it was mapped
  if (buffer.mappedPtr) {
    buffer.buffer->Unmap(0, nullptr);
  }

  // Add to in-flight list
  BufferEntry entry;
  entry.buffer = buffer.buffer;
  entry.size = buffer.size;
  entry.fenceValue = fenceValue;

  // Determine if it's a readback buffer
  D3D12_HEAP_PROPERTIES heapProps;
  buffer.buffer->GetHeapProperties(&heapProps, nullptr);
  entry.isReadback = (heapProps.Type == D3D12_HEAP_TYPE_READBACK);

  inFlightBuffers_.push_back(std::move(entry));
}

void D3D12StagingDevice::reclaimCompletedBuffers() {
  // Note: Internal helper called by allocate* methods with poolMutex_ already held
  if (!fence_) {
    return;
  }

  const uint64_t completedValue = fence_->GetCompletedValue();

  // Move completed buffers from in-flight to available
  auto it = inFlightBuffers_.begin();
  while (it != inFlightBuffers_.end()) {
    if (it->fenceValue <= completedValue) {
      availableBuffers_.push_back({it->buffer, it->size, 0, it->isReadback});
      it = inFlightBuffers_.erase(it);
    } else {
      ++it;
    }
  }
}

Result D3D12StagingDevice::createStagingBuffer(
    size_t size,
    bool forReadback,
    igl::d3d12::ComPtr<ID3D12Resource>* outBuffer) {
  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = forReadback ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  D3D12_RESOURCE_STATES initialState = forReadback ? D3D12_RESOURCE_STATE_COPY_DEST
                                                    : D3D12_RESOURCE_STATE_GENERIC_READ;

  HRESULT hr = device_->CreateCommittedResource(&heapProps,
                                                D3D12_HEAP_FLAG_NONE,
                                                &desc,
                                                initialState,
                                                nullptr,
                                                IID_PPV_ARGS(outBuffer->GetAddressOf()));

  if (FAILED(hr)) {
    return Result{Result::Code::RuntimeError, "Failed to create staging buffer"};
  }

  IGL_D3D12_LOG_VERBOSE("D3D12StagingDevice: Created new %s buffer (size: %zu bytes)\n",
               forReadback ? "readback" : "upload",
               size);

  return Result{};
}

bool D3D12StagingDevice::findReusableBuffer(size_t size,
                                            bool forReadback,
                                            igl::d3d12::ComPtr<ID3D12Resource>* outBuffer) {
  // Find a buffer that matches type and is large enough
  for (auto it = availableBuffers_.begin(); it != availableBuffers_.end(); ++it) {
    if (it->isReadback == forReadback && it->size >= size) {
      // Prefer buffers that are close in size (within 2x) to avoid waste
      if (it->size <= size * 2) {
        *outBuffer = it->buffer;
        availableBuffers_.erase(it);
        return true;
      }
    }
  }

  return false;
}

} // namespace igl::d3d12
