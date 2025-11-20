/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12AllocatorPool.h>

#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/D3D12FenceWaiter.h>
#include <igl/d3d12/D3D12ImmediateCommands.h>
#include <igl/d3d12/D3D12StagingDevice.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/UploadRingBuffer.h>

namespace igl::d3d12 {

void D3D12AllocatorPool::initialize(D3D12Context& ctx, IFenceProvider* fenceProvider) {
  auto* device = ctx.getDevice();
  if (!device) {
    IGL_LOG_ERROR("D3D12AllocatorPool::initialize: D3D12 device is null\n");
    return;
  }

  HRESULT hr = device->CreateFence(
      0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(uploadFence_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR(
        "D3D12AllocatorPool::initialize: Failed to create upload fence: 0x%08X\n",
        hr);
  } else {
    uploadFenceValue_ = 0;
    IGL_D3D12_LOG_VERBOSE(
        "D3D12AllocatorPool::initialize: Upload fence created successfully\n");
  }

  constexpr uint64_t kUploadRingBufferSize = 128ull * 1024ull * 1024ull;
  uploadRingBuffer_ =
      std::make_unique<UploadRingBuffer>(device, kUploadRingBufferSize);

  auto* commandQueue = ctx.getCommandQueue();
  if (commandQueue && uploadFence_.Get() && fenceProvider) {
    immediateCommands_ = std::make_unique<D3D12ImmediateCommands>(
        device, commandQueue, uploadFence_.Get(), fenceProvider);
    stagingDevice_ = std::make_unique<D3D12StagingDevice>(
        device, uploadFence_.Get(), uploadRingBuffer_.get());
  }
}

void D3D12AllocatorPool::processCompletedUploads() {
  if (!uploadFence_.Get()) {
    return;
  }

  const UINT64 completed = uploadFence_->GetCompletedValue();

  {
    std::lock_guard<std::mutex> lock(pendingUploadsMutex_);
    auto it = pendingUploads_.begin();
    while (it != pendingUploads_.end()) {
      if (it->fenceValue <= completed) {
        it = pendingUploads_.erase(it);
      } else {
        ++it;
      }
    }
  }

  if (uploadRingBuffer_) {
    uploadRingBuffer_->retire(completed);
  }
}

void D3D12AllocatorPool::trackUploadBuffer(
    ComPtr<ID3D12Resource> buffer,
    UINT64 fenceValue) {
  if (!buffer.Get()) {
    return;
  }

  std::lock_guard<std::mutex> lock(pendingUploadsMutex_);
  pendingUploads_.push_back(PendingUpload{fenceValue, std::move(buffer)});
}

ComPtr<ID3D12CommandAllocator> D3D12AllocatorPool::getUploadCommandAllocator(
    D3D12Context& ctx) {
  if (!uploadFence_.Get()) {
    IGL_LOG_ERROR(
        "D3D12AllocatorPool::getUploadCommandAllocator: Upload fence not "
        "initialized\n");
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(commandAllocatorPoolMutex_);

  const UINT64 completedValue = uploadFence_->GetCompletedValue();

  for (size_t i = 0; i < commandAllocatorPool_.size(); ++i) {
    auto& tracked = commandAllocatorPool_[i];

    if (completedValue >= tracked.fenceValue) {
      auto allocator = tracked.allocator;

      commandAllocatorPool_[i] = commandAllocatorPool_.back();
      commandAllocatorPool_.pop_back();

      HRESULT hr = allocator->Reset();
      if (FAILED(hr)) {
        IGL_LOG_ERROR(
            "D3D12AllocatorPool::getUploadCommandAllocator: "
            "CommandAllocator::Reset failed: 0x%08X\n",
            hr);
        return nullptr;
      }

      totalAllocatorReuses_++;
      return allocator;
    }
  }

  static constexpr size_t kMaxCommandAllocators = 256;

  if (totalCommandAllocatorsCreated_ >= kMaxCommandAllocators) {
    IGL_LOG_ERROR(
        "D3D12AllocatorPool::getUploadCommandAllocator: Command allocator "
        "pool exhausted\n");
    return nullptr;
  }

  auto* device = ctx.getDevice();
  if (!device) {
    IGL_LOG_ERROR(
        "D3D12AllocatorPool::getUploadCommandAllocator: D3D12 device is null\n");
    return nullptr;
  }

  ComPtr<ID3D12CommandAllocator> newAllocator;
  HRESULT hr = device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(newAllocator.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR(
        "D3D12AllocatorPool::getUploadCommandAllocator: "
        "CreateCommandAllocator failed: 0x%08X\n",
        hr);
    return nullptr;
  }

  totalCommandAllocatorsCreated_++;
  return newAllocator;
}

void D3D12AllocatorPool::returnUploadCommandAllocator(
    ComPtr<ID3D12CommandAllocator> allocator,
    UINT64 fenceValue) {
  if (!allocator.Get()) {
    return;
  }

  std::lock_guard<std::mutex> lock(commandAllocatorPoolMutex_);

  TrackedCommandAllocator tracked;
  tracked.allocator = allocator;
  tracked.fenceValue = fenceValue;
  commandAllocatorPool_.push_back(tracked);

  if (commandAllocatorPool_.size() > peakPoolSize_) {
    peakPoolSize_ = commandAllocatorPool_.size();
  }
}

::igl::Result D3D12AllocatorPool::waitForUploadFence(
    const Device& device,
    UINT64 fenceValue) const {
  if (!uploadFence_.Get()) {
    return ::igl::Result(
        ::igl::Result::Code::InvalidOperation, "Upload fence not initialized");
  }

  if (uploadFence_->GetCompletedValue() >= fenceValue) {
    return ::igl::Result();
  }

  FenceWaiter waiter(uploadFence_.Get(), fenceValue);
  ::igl::Result waitResult = waiter.wait();
  if (!waitResult.isOk()) {
    ::igl::Result deviceStatus = device.checkDeviceRemoval();
    if (!deviceStatus.isOk()) {
      return deviceStatus;
    }
    return waitResult;
  }

  return Result();
}

void D3D12AllocatorPool::clearOnDeviceDestruction() {
  {
    std::lock_guard<std::mutex> lock(commandAllocatorPoolMutex_);
    commandAllocatorPool_.clear();
    totalCommandAllocatorsCreated_ = 0;
    peakPoolSize_ = 0;
    totalAllocatorReuses_ = 0;
  }
  {
    std::lock_guard<std::mutex> lock(pendingUploadsMutex_);
    pendingUploads_.clear();
  }

  uploadRingBuffer_.reset();
  stagingDevice_.reset();
  immediateCommands_.reset();
}

} // namespace igl::d3d12
