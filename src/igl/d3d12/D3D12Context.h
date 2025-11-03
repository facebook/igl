/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>
#include <memory>

namespace igl {
class IBuffer; // Forward declaration for igl::IBuffer
}

namespace igl::d3d12 {

class DescriptorHeapManager; // fwd decl in igl::d3d12

// Per-frame context for CPU/GPU parallelism
struct FrameContext {
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  UINT64 fenceValue = 0;

  // Per-frame shader-visible descriptor heaps (following Microsoft MiniEngine pattern)
  // Each frame gets its own isolated heaps to prevent descriptor conflicts
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;  // 1024 descriptors (MiniEngine size)
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap;     // 32 descriptors (kMaxSamplers)

  // Linear allocator counters - reset to 0 each frame
  // Incremented by each command buffer's encoders as they allocate descriptors
  uint32_t nextCbvSrvUavDescriptor = 0;
  uint32_t nextSamplerDescriptor = 0;

  // Transient resources that must be kept alive until this frame completes GPU execution
  // Examples: push constant buffers, temporary upload buffers
  // CRITICAL: These are cleared when we advance to the next frame AFTER waiting for
  // this frame's fence, ensuring the GPU has finished reading them
  std::vector<std::shared_ptr<igl::IBuffer>> transientBuffers;
};

class D3D12Context {
 public:
  D3D12Context() = default;
  ~D3D12Context();

  Result initialize(HWND hwnd, uint32_t width, uint32_t height);
  Result resize(uint32_t width, uint32_t height);

  ID3D12Device* getDevice() const { return device_.Get(); }
  ID3D12CommandQueue* getCommandQueue() const { return commandQueue_.Get(); }
  IDXGISwapChain3* getSwapChain() const { return swapChain_.Get(); }

  // Get descriptor heap for current frame
  ID3D12DescriptorHeap* getCbvSrvUavHeap() const {
    return frameContexts_[currentFrameIndex_].cbvSrvUavHeap.Get();
  }
  ID3D12DescriptorHeap* getSamplerHeap() const {
    return frameContexts_[currentFrameIndex_].samplerHeap.Get();
  }

  // Get descriptor sizes
  UINT getCbvSrvUavDescriptorSize() const { return cbvSrvUavDescriptorSize_; }
  UINT getSamplerDescriptorSize() const { return samplerDescriptorSize_; }

  // Get descriptor handles from per-frame heaps
  D3D12_CPU_DESCRIPTOR_HANDLE getCbvSrvUavCpuHandle(uint32_t descriptorIndex) const {
    auto h = frameContexts_[currentFrameIndex_].cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * cbvSrvUavDescriptorSize_;
    return h;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE getCbvSrvUavGpuHandle(uint32_t descriptorIndex) const {
    auto h = frameContexts_[currentFrameIndex_].cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * cbvSrvUavDescriptorSize_;
    return h;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE getSamplerCpuHandle(uint32_t descriptorIndex) const {
    auto h = frameContexts_[currentFrameIndex_].samplerHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * samplerDescriptorSize_;
    return h;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE getSamplerGpuHandle(uint32_t descriptorIndex) const {
    auto h = frameContexts_[currentFrameIndex_].samplerHeap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * samplerDescriptorSize_;
    return h;
  }

  // Optional descriptor heap manager (provided by headless context)
  DescriptorHeapManager* getDescriptorHeapManager() const { return heapMgr_; }

  uint32_t getCurrentBackBufferIndex() const;
  ID3D12Resource* getCurrentBackBuffer() const;
  D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTV() const;

  void waitForGPU();

  // Per-frame fence access for CommandQueue
  FrameContext* getFrameContexts() { return frameContexts_; }
  UINT& getCurrentFrameIndex() { return currentFrameIndex_; }
  UINT64& getFenceValue() { return fenceValue_; }
  ID3D12Fence* getFence() const { return fence_.Get(); }
  HANDLE getFenceEvent() const { return fenceEvent_; }

 protected:
  void createDevice();
  void createCommandQueue();
  void createSwapChain(HWND hwnd, uint32_t width, uint32_t height);
  void createRTVHeap();
  void createBackBuffers();
  void createDescriptorHeaps();

  Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[kMaxFramesInFlight];
  UINT rtvDescriptorSize_ = 0;

  // Descriptor sizes (cached from device)
  UINT cbvSrvUavDescriptorSize_ = 0;
  UINT samplerDescriptorSize_ = 0;

  // Descriptor heap manager for headless contexts (unit tests)
  DescriptorHeapManager* ownedHeapMgr_ = nullptr;  // Owned manager for windowed contexts (raw ptr, manually deleted)
  DescriptorHeapManager* heapMgr_ = nullptr; // non-owning; points to ownedHeapMgr_ or external (headless)

  // Per-frame synchronization for CPU/GPU parallelism
  FrameContext frameContexts_[kMaxFramesInFlight];
  UINT currentFrameIndex_ = 0;

  // Global synchronization
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  UINT64 fenceValue_ = 0;
  HANDLE fenceEvent_ = nullptr;

  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

} // namespace igl::d3d12
