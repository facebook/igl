/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/UploadRingBuffer.h>
#include <cstring>

namespace igl::d3d12 {

namespace {
constexpr D3D12_RESOURCE_DESC makeBufferDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment = 0;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  desc.Flags = flags;
  return desc;
}
} // namespace

Buffer::Buffer(Device& device,
               Microsoft::WRL::ComPtr<ID3D12Resource> resource,
               const BufferDesc& desc,
               D3D12_RESOURCE_STATES initialState)
    : device_(&device),
      resource_(std::move(resource)),
      desc_(desc),
      defaultState_(computeDefaultState(desc)),
      currentState_(initialState) {
  // Determine storage type based on heap properties
  if (resource_.Get()) {
    D3D12_HEAP_PROPERTIES heapProps;
    D3D12_HEAP_FLAGS heapFlags;
    resource_->GetHeapProperties(&heapProps, &heapFlags);

    if (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD) {
      storage_ = ResourceStorage::Shared;
    } else if (heapProps.Type == D3D12_HEAP_TYPE_READBACK) {
      storage_ = ResourceStorage::Shared;
    } else {
      storage_ = ResourceStorage::Private;
    }

    if (storage_ != ResourceStorage::Private) {
      currentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

    // Track resource creation
    D3D12Context::trackResourceCreation("Buffer", desc_.length);
  }
}

Buffer::~Buffer() {
  ULONG refCount = 0;
  if (resource_.Get()) {
    resource_.Get()->AddRef();
    refCount = resource_.Get()->Release();  // Returns count AFTER the Release

    // Track resource destruction
    D3D12Context::trackResourceDestruction("Buffer", desc_.length);
  }
  IGL_LOG_INFO("Buffer::~Buffer() - Destroying buffer, resource_=%p, final_refCount_before_ComPtr_dtor=%lu\n", resource_.Get(), refCount);
  if (mappedPtr_) {
    unmap();
  }
  // ComPtr destructor will now release, bringing refcount to refCount-1 (which should be 0)
}

Result Buffer::upload(const void* data, const BufferRange& range) {
  if (resource_.Get() == nullptr) {
    return Result(Result::Code::ArgumentInvalid, "Buffer resource is null");
  }

  if (!data) {
    IGL_LOG_ERROR("Buffer::upload: data is NULL!\n");
    return Result(Result::Code::ArgumentInvalid, "Upload data is null");
  }

  // Validate range
  if (range.size == 0 || range.offset + range.size > desc_.length) {
    return Result(Result::Code::ArgumentOutOfRange, "Upload range is out of bounds");
  }

  // For UPLOAD heap, map, copy, unmap
  if (storage_ == ResourceStorage::Shared) {
    void* mappedData = nullptr;
    D3D12_RANGE readRange = {0, 0}; // Not reading from GPU

    HRESULT hr = resource_->Map(0, &readRange, &mappedData);
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to map buffer");
    }

    uint8_t* dest = static_cast<uint8_t*>(mappedData) + range.offset;
    std::memcpy(dest, data, range.size);

    D3D12_RANGE writtenRange = {range.offset, range.offset + range.size};
    resource_->Unmap(0, &writtenRange);

    return Result();
  }

  // For DEFAULT heap, need upload via intermediate buffer
  if (!device_) {
    return Result(Result::Code::RuntimeError, "Buffer device is null");
  }

  auto& ctx = device_->getD3D12Context();
  ID3D12Device* d3dDevice = ctx.getDevice();
  ID3D12CommandQueue* queue = ctx.getCommandQueue();
  if (!d3dDevice || !queue) {
    return Result(Result::Code::RuntimeError, "D3D12 device or command queue unavailable");
  }

  // Reclaim completed upload buffers before allocating new ones.
  device_->processCompletedUploads();

  // P1_DX12-009: Try to allocate from upload ring buffer first
  UploadRingBuffer* ringBuffer = device_->getUploadRingBuffer();
  UploadRingBuffer::Allocation ringAllocation;
  bool useRingBuffer = false;

  // Get fence value that will signal when this upload completes
  const UINT64 uploadFenceValue = device_->getNextUploadFenceValue();

  if (ringBuffer) {
    // D3D12 requires 256-byte alignment for constant buffers
    constexpr uint64_t kUploadAlignment = 256;
    ringAllocation = ringBuffer->allocate(range.size, kUploadAlignment, uploadFenceValue);

    if (ringAllocation.valid) {
      // Successfully allocated from ring buffer
      std::memcpy(ringAllocation.cpuAddress, data, range.size);
      useRingBuffer = true;
    }
  }

  // Fallback: create temporary upload buffer if ring buffer allocation failed
  Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
  uint64_t uploadBufferOffset = 0;
  HRESULT hr = S_OK;

  if (!useRingBuffer) {
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    const auto uploadDesc = makeBufferDesc(range.size);
    hr = d3dDevice->CreateCommittedResource(&uploadHeapProps,
                                                    D3D12_HEAP_FLAG_NONE,
                                                    &uploadDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    nullptr,
                                                    IID_PPV_ARGS(uploadBuffer.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create upload buffer");
    }

    void* mapped = nullptr;
    D3D12_RANGE rr = {0, 0};
    hr = uploadBuffer->Map(0, &rr, &mapped);
    if (FAILED(hr) || mapped == nullptr) {
      return Result(Result::Code::RuntimeError, "Failed to map upload buffer");
    }
    std::memcpy(mapped, data, range.size);
    uploadBuffer->Unmap(0, nullptr);
  }

  // P0_DX12-005: Get command allocator from pool with fence tracking
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator = device_->getUploadCommandAllocator();
  if (!allocator.Get()) {
    return Result(Result::Code::RuntimeError, "Failed to get command allocator from pool");
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  hr = d3dDevice->CreateCommandList(0,
                                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    allocator.Get(),
                                    nullptr,
                                    IID_PPV_ARGS(cmdList.GetAddressOf()));
  if (FAILED(hr)) {
    // Return allocator to pool with fence value 0 (immediately available)
    device_->returnUploadCommandAllocator(allocator, 0);
    return Result(Result::Code::RuntimeError, "Failed to create command list for upload");
  }

  if (currentState_ != D3D12_RESOURCE_STATE_COPY_DEST) {
    D3D12_RESOURCE_BARRIER toCopyDest = {};
    toCopyDest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopyDest.Transition.pResource = resource_.Get();
    toCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toCopyDest.Transition.StateBefore = currentState_;
    toCopyDest.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    cmdList->ResourceBarrier(1, &toCopyDest);
  }

  // Copy from either ring buffer or temporary upload buffer
  if (useRingBuffer) {
    cmdList->CopyBufferRegion(resource_.Get(), range.offset,
                              ringBuffer->getUploadHeap(), ringAllocation.offset,
                              range.size);
  } else {
    cmdList->CopyBufferRegion(resource_.Get(), range.offset, uploadBuffer.Get(), 0, range.size);
  }

  D3D12_RESOURCE_STATES postState =
      (defaultState_ == D3D12_RESOURCE_STATE_COMMON) ? D3D12_RESOURCE_STATE_GENERIC_READ : defaultState_;

  if (postState != D3D12_RESOURCE_STATE_COPY_DEST) {
    D3D12_RESOURCE_BARRIER toDefault = {};
    toDefault.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toDefault.Transition.pResource = resource_.Get();
    toDefault.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toDefault.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toDefault.Transition.StateAfter = postState;
    cmdList->ResourceBarrier(1, &toDefault);
    currentState_ = postState;
  } else {
    currentState_ = D3D12_RESOURCE_STATE_COPY_DEST;
  }

  hr = cmdList->Close();
  if (FAILED(hr)) {
    // Return allocator to pool with fence value 0 (immediately available)
    device_->returnUploadCommandAllocator(allocator, 0);
    return Result(Result::Code::RuntimeError, "Failed to close upload command list");
  }

  ID3D12CommandList* lists[] = {cmdList.Get()};
  queue->ExecuteCommandLists(1, lists);

  // P0_DX12-005: Signal upload fence and return allocator to pool with fence value
  // P1_DX12-009: Use pre-allocated uploadFenceValue (already incremented for ring buffer)
  // CRITICAL: This ensures the allocator is not reused until GPU completes execution
  ID3D12Fence* uploadFence = device_->getUploadFence();

  hr = queue->Signal(uploadFence, uploadFenceValue);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("Buffer::upload: Failed to signal upload fence: 0x%08X\n", hr);
    // Still return allocator, but with 0 to avoid blocking the pool
    device_->returnUploadCommandAllocator(allocator, 0);
  } else {
    // Return allocator to pool with fence value (will be reused after fence signaled)
    device_->returnUploadCommandAllocator(allocator, uploadFenceValue);
  }

  // Only track temporary upload buffers (ring buffer is persistent)
  // DX12-NEW-02: Pass uploadFenceValue (already signaled above) to track with correct fence
  if (!useRingBuffer && uploadBuffer.Get()) {
    device_->trackUploadBuffer(std::move(uploadBuffer), uploadFenceValue);
  }

  return Result();
}

void* Buffer::map(const BufferRange& range, Result* IGL_NULLABLE outResult) {
  if (resource_.Get() == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Buffer resource is null");
    return nullptr;
  }

  // Validate range
  if (range.offset > desc_.length || range.size > desc_.length ||
      (range.offset + range.size) > desc_.length) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Map range is out of bounds");
    return nullptr;
  }

  // Handle mapping of DEFAULT heap storage buffers requested as Shared
  // This happens when compute shader output buffers need to be read back
  const bool isStorageBuffer = (desc_.type & BufferDesc::BufferTypeBits::Storage) != 0;
  const bool requestedShared = (desc_.storage == ResourceStorage::Shared ||
                                desc_.storage == ResourceStorage::Managed);
  const bool needsReadbackStaging = (storage_ != ResourceStorage::Shared) &&
                                    isStorageBuffer && requestedShared;

  if (needsReadbackStaging) {
    // Storage buffer in DEFAULT heap but requested as Shared - need staging
    if (!device_) {
      Result::setResult(outResult, Result::Code::RuntimeError, "Device is null");
      return nullptr;
    }

    auto& ctx = device_->getD3D12Context();
    auto* d3dDevice = ctx.getDevice();
    auto* queue = ctx.getCommandQueue();

    if (!d3dDevice || !queue) {
      Result::setResult(outResult, Result::Code::RuntimeError, "D3D12 device or queue is null");
      return nullptr;
    }

    // Create READBACK staging buffer if not already created
    if (!readbackStagingBuffer_.Get()) {
      D3D12_HEAP_PROPERTIES readbackHeap = {};
      readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
      readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

      D3D12_RESOURCE_DESC bufferDesc = {};
      bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      bufferDesc.Alignment = 0;
      bufferDesc.Width = desc_.length;
      bufferDesc.Height = 1;
      bufferDesc.DepthOrArraySize = 1;
      bufferDesc.MipLevels = 1;
      bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
      bufferDesc.SampleDesc.Count = 1;
      bufferDesc.SampleDesc.Quality = 0;
      bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

      HRESULT hr = d3dDevice->CreateCommittedResource(
          &readbackHeap,
          D3D12_HEAP_FLAG_NONE,
          &bufferDesc,
          D3D12_RESOURCE_STATE_COPY_DEST,
          nullptr,
          IID_PPV_ARGS(readbackStagingBuffer_.GetAddressOf()));

      if (FAILED(hr)) {
        Result::setResult(outResult, Result::Code::RuntimeError,
                         "Failed to create readback staging buffer");
        return nullptr;
      }
    }

    // ALWAYS copy from DEFAULT buffer to readback staging when mapping
    // The DEFAULT buffer content may have changed since the last map() call
    // (e.g., via copyTextureToBuffer or compute shader writes)
    IGL_LOG_INFO("Buffer::map() - Copying from DEFAULT buffer (resource=%p) to readback staging\n",
                 resource_.Get());
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;

    if (FAILED(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 IID_PPV_ARGS(allocator.GetAddressOf()))) ||
        FAILED(d3dDevice->CreateCommandList(0,
                                            D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            allocator.Get(),
                                            nullptr,
                                            IID_PPV_ARGS(cmdList.GetAddressOf())))) {
      Result::setResult(outResult, Result::Code::RuntimeError,
                       "Failed to create command list for buffer copy");
      return nullptr;
    }

    // Transition source buffer to COPY_SOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    // Copy entire buffer
    cmdList->CopyBufferRegion(readbackStagingBuffer_.Get(), 0, resource_.Get(), 0, desc_.length);

    // Transition back
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->Close();
    ID3D12CommandList* lists[] = {cmdList.Get()};
    queue->ExecuteCommandLists(1, lists);

    // Wait for copy to complete
    ctx.waitForGPU();

    // Map the READBACK staging buffer
    D3D12_RANGE readRange = {static_cast<SIZE_T>(range.offset),
                            static_cast<SIZE_T>(range.offset + range.size)};
    HRESULT hr = readbackStagingBuffer_->Map(0, &readRange, &mappedPtr_);

    if (FAILED(hr)) {
      Result::setResult(outResult, Result::Code::RuntimeError, "Failed to map readback staging buffer");
      return nullptr;
    }

    Result::setOk(outResult);
    return static_cast<uint8_t*>(mappedPtr_) + range.offset;
  }

  // Standard path for UPLOAD/READBACK heap buffers
  if (storage_ != ResourceStorage::Shared) {
    Result::setResult(outResult, Result::Code::Unsupported,
                      "Cannot map GPU-only buffer (use ResourceStorage::Shared)");
    return nullptr;
  }

  if (mappedPtr_) {
    // Already mapped, return offset pointer
    Result::setOk(outResult);
    return static_cast<uint8_t*>(mappedPtr_) + range.offset;
  }

  D3D12_RANGE readRange = {0, 0}; // Not reading from GPU
  HRESULT hr = resource_->Map(0, &readRange, &mappedPtr_);

  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to map buffer");
    return nullptr;
  }

  Result::setOk(outResult);
  return static_cast<uint8_t*>(mappedPtr_) + range.offset;
}

void Buffer::unmap() {
  if (!mappedPtr_) {
    return;
  }

  // Unmap the appropriate resource (staging buffer or main buffer)
  if (readbackStagingBuffer_.Get()) {
    readbackStagingBuffer_->Unmap(0, nullptr);
  } else if (resource_.Get()) {
    resource_->Unmap(0, nullptr);
  }

  mappedPtr_ = nullptr;
}

BufferDesc::BufferAPIHint Buffer::requestedApiHints() const noexcept {
  return desc_.hint;
}

BufferDesc::BufferAPIHint Buffer::acceptedApiHints() const noexcept {
  return desc_.hint;
}

ResourceStorage Buffer::storage() const noexcept {
  return storage_;
}

size_t Buffer::getSizeInBytes() const {
  return desc_.length;
}

uint64_t Buffer::gpuAddress(size_t offset) const {
  if (resource_.Get() == nullptr) {
    return 0;
  }

  return resource_->GetGPUVirtualAddress() + offset;
}

BufferDesc::BufferType Buffer::getBufferType() const {
  return desc_.type;
}

D3D12_RESOURCE_STATES Buffer::computeDefaultState(const BufferDesc& desc) const {
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

  if ((desc.type & BufferDesc::BufferTypeBits::Storage) != 0) {
    state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  }
  if ((desc.type & BufferDesc::BufferTypeBits::Vertex) != 0 ||
      (desc.type & BufferDesc::BufferTypeBits::Uniform) != 0) {
    state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  }
  if ((desc.type & BufferDesc::BufferTypeBits::Index) != 0) {
    state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
  }
  if ((desc.type & BufferDesc::BufferTypeBits::Indirect) != 0) {
    state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  }

  if (state == D3D12_RESOURCE_STATE_COMMON) {
    return D3D12_RESOURCE_STATE_GENERIC_READ;
  }

  // Remove COMMON bit if other bits are set.
  state &= ~D3D12_RESOURCE_STATE_COMMON;
  return state;
}

} // namespace igl::d3d12
