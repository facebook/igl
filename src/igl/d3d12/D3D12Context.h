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
#include <mutex>

namespace igl {
class IBuffer; // Forward declaration for igl::IBuffer
}

namespace igl::d3d12 {

class DescriptorHeapManager; // fwd decl in igl::d3d12

// Descriptor heap page for dynamic growth (C-001)
// Following Microsoft MiniEngine's DynamicDescriptorHeap pattern
struct DescriptorHeapPage {
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
  uint32_t capacity;  // Total descriptors in this page
  uint32_t used;      // Currently allocated descriptors

  DescriptorHeapPage() : capacity(0), used(0) {}
  DescriptorHeapPage(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> h, uint32_t cap)
      : heap(h), capacity(cap), used(0) {}
};

// Per-frame context for CPU/GPU parallelism
struct FrameContext {
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  UINT64 fenceValue = 0;

  // Per-frame shader-visible descriptor heaps (following Microsoft MiniEngine pattern)
  // C-001: Now supports multiple pages for dynamic growth to prevent overflow corruption
  // Each frame gets its own isolated heap pages to prevent descriptor conflicts
  std::vector<DescriptorHeapPage> cbvSrvUavHeapPages;  // Dynamic array of 1024-descriptor pages
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap;     // 2048 descriptors (kMaxSamplers)

  // Current active page index for CBV/SRV/UAV allocation
  uint32_t currentCbvSrvUavPageIndex = 0;

  // Legacy accessor for backward compatibility (returns first page)
  // DEPRECATED: Use cbvSrvUavHeapPages directly for multi-page access
  ID3D12DescriptorHeap* cbvSrvUavHeap() const {
    return cbvSrvUavHeapPages.empty() ? nullptr : cbvSrvUavHeapPages[0].heap.Get();
  }

  // Linear allocator counters - reset to 0 each frame
  // Incremented by each command buffer's encoders as they allocate descriptors
  uint32_t nextCbvSrvUavDescriptor = 0;
  uint32_t nextSamplerDescriptor = 0;

  // Transient resources that must be kept alive until this frame completes GPU execution
  // Examples: push constant buffers, temporary upload buffers
  // CRITICAL: These are cleared when we advance to the next frame AFTER waiting for
  // this frame's fence, ensuring the GPU has finished reading them
  std::vector<std::shared_ptr<igl::IBuffer>> transientBuffers;
  std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> transientResources;

  // Telemetry for transient resource tracking (P2_DX12-120)
  // Tracks high-water mark to observe peak usage and detect unbounded growth
  size_t transientBuffersHighWater = 0;
  size_t transientResourcesHighWater = 0;

  // Telemetry for descriptor heap usage tracking (P0_DX12-FIND-02)
  // Tracks peak descriptor usage per frame to detect heap overflow risks
  uint32_t peakCbvSrvUavUsage = 0;
  uint32_t peakSamplerUsage = 0;
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
  // C-001: Returns first page for backward compatibility, but prefer using getFrameContexts()
  ID3D12DescriptorHeap* getCbvSrvUavHeap() const {
    return frameContexts_[currentFrameIndex_].cbvSrvUavHeap();
  }
  ID3D12DescriptorHeap* getSamplerHeap() const {
    return frameContexts_[currentFrameIndex_].samplerHeap.Get();
  }

  // C-001: Allocate a new descriptor heap page for dynamic growth
  Result allocateDescriptorHeapPage(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                     uint32_t numDescriptors,
                                     Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>* outHeap);

  // Get descriptor sizes
  UINT getCbvSrvUavDescriptorSize() const { return cbvSrvUavDescriptorSize_; }
  UINT getSamplerDescriptorSize() const { return samplerDescriptorSize_; }

  // Get root signature capabilities
  D3D_ROOT_SIGNATURE_VERSION getHighestRootSignatureVersion() const { return highestRootSignatureVersion_; }
  D3D12_RESOURCE_BINDING_TIER getResourceBindingTier() const { return resourceBindingTier_; }

  // Get shader model capability (H-010)
  D3D_SHADER_MODEL getMaxShaderModel() const { return maxShaderModel_; }

  // Get tearing support capability
  bool isTearingSupported() const { return tearingSupported_; }

  // Get command signatures for indirect drawing (P3_DX12-FIND-13)
  ID3D12CommandSignature* getDrawIndirectSignature() const { return drawIndirectSignature_.Get(); }
  ID3D12CommandSignature* getDrawIndexedIndirectSignature() const { return drawIndexedIndirectSignature_.Get(); }

  // Get descriptor handles from per-frame heaps
  // C-001: Now uses current page for multi-heap support
  D3D12_CPU_DESCRIPTOR_HANDLE getCbvSrvUavCpuHandle(uint32_t descriptorIndex) const {
    const auto& frameCtx = frameContexts_[currentFrameIndex_];
    const auto& pages = frameCtx.cbvSrvUavHeapPages;
    const uint32_t pageIdx = frameCtx.currentCbvSrvUavPageIndex;

    if (pages.empty() || pageIdx >= pages.size()) {
      return {0};  // Invalid handle
    }

    auto h = pages[pageIdx].heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * cbvSrvUavDescriptorSize_;
    return h;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE getCbvSrvUavGpuHandle(uint32_t descriptorIndex) const {
    const auto& frameCtx = frameContexts_[currentFrameIndex_];
    const auto& pages = frameCtx.cbvSrvUavHeapPages;
    const uint32_t pageIdx = frameCtx.currentCbvSrvUavPageIndex;

    if (pages.empty() || pageIdx >= pages.size()) {
      return {0};  // Invalid handle
    }

    auto h = pages[pageIdx].heap->GetGPUDescriptorHandleForHeapStart();
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

  // Resource tracking for diagnostics
  static void trackResourceCreation(const char* type, size_t sizeBytes);
  static void trackResourceDestruction(const char* type, size_t sizeBytes);
  static void logResourceStats();

 protected:
  void createDevice();
  void createCommandQueue();
  void createSwapChain(HWND hwnd, uint32_t width, uint32_t height);
  Result recreateSwapChain(uint32_t width, uint32_t height);
  void createRTVHeap();
  void createBackBuffers();
  void createDescriptorHeaps();
  void createCommandSignatures();

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

  // Feature detection for root signature capabilities
  D3D_ROOT_SIGNATURE_VERSION highestRootSignatureVersion_ = D3D_ROOT_SIGNATURE_VERSION_1_0;
  D3D12_RESOURCE_BINDING_TIER resourceBindingTier_ = D3D12_RESOURCE_BINDING_TIER_1;

  // Feature detection for shader model (H-010)
  // FL11 hardware guarantees SM 5.1 minimum
  D3D_SHADER_MODEL maxShaderModel_ = D3D_SHADER_MODEL_5_1;

  // Feature detection for variable refresh rate (tearing) support
  bool tearingSupported_ = false;

  // Command signatures for indirect drawing (P3_DX12-FIND-13)
  Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawIndirectSignature_;
  Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawIndexedIndirectSignature_;

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

  // Resource tracking (static for global tracking across all contexts)
  struct ResourceStats {
    size_t totalBuffersCreated = 0;
    size_t totalBuffersDestroyed = 0;
    size_t totalTexturesCreated = 0;
    size_t totalTexturesDestroyed = 0;
    size_t totalSRVsCreated = 0;
    size_t totalSamplersCreated = 0;
    size_t bufferMemoryBytes = 0;
    size_t textureMemoryBytes = 0;
  };
  static ResourceStats resourceStats_;
  static std::mutex resourceStatsMutex_;
};

} // namespace igl::d3d12
