/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <igl/d3d12/Common.h>

namespace igl {
class Result;
} // namespace igl

namespace igl::d3d12 {

class Device;
class D3D12Context;
class UploadRingBuffer;
class D3D12ImmediateCommands;
class D3D12StagingDevice;
class IFenceProvider;

class D3D12AllocatorPool {
 public:
  D3D12AllocatorPool() = default;

  void initialize(D3D12Context& ctx, IFenceProvider* fenceProvider);

  void processCompletedUploads();
  void trackUploadBuffer(ComPtr<ID3D12Resource> buffer, UINT64 fenceValue);

  ComPtr<ID3D12CommandAllocator> getUploadCommandAllocator(D3D12Context& ctx);
  void returnUploadCommandAllocator(ComPtr<ID3D12CommandAllocator> allocator, UINT64 fenceValue);

  ID3D12Fence* getUploadFence() const {
    return uploadFence_.Get();
  }

  UINT64 getNextUploadFenceValue() {
    return ++uploadFenceValue_;
  }

  UINT64 getLastUploadFenceValue() const {
    return uploadFenceValue_;
  }

  UploadRingBuffer* getUploadRingBuffer() const {
    return uploadRingBuffer_.get();
  }

  D3D12ImmediateCommands* getImmediateCommands() const {
    return immediateCommands_.get();
  }

  D3D12StagingDevice* getStagingDevice() const {
    return stagingDevice_.get();
  }

  ::igl::Result waitForUploadFence(const Device& device, UINT64 fenceValue) const;

  void clearOnDeviceDestruction();

 private:
  struct PendingUpload {
    UINT64 fenceValue = 0;
    ComPtr<ID3D12Resource> resource;
  };

  struct TrackedCommandAllocator {
    ComPtr<ID3D12CommandAllocator> allocator;
    UINT64 fenceValue = 0;
  };

  std::mutex pendingUploadsMutex_;
  std::vector<PendingUpload> pendingUploads_;

  std::mutex commandAllocatorPoolMutex_;
  std::vector<TrackedCommandAllocator> commandAllocatorPool_;
  size_t totalCommandAllocatorsCreated_ = 0;
  size_t peakPoolSize_ = 0;
  size_t totalAllocatorReuses_ = 0;

  ComPtr<ID3D12Fence> uploadFence_;
  UINT64 uploadFenceValue_ = 0;

  std::unique_ptr<UploadRingBuffer> uploadRingBuffer_;
  std::unique_ptr<D3D12ImmediateCommands> immediateCommands_;
  std::unique_ptr<D3D12StagingDevice> stagingDevice_;
};

} // namespace igl::d3d12
