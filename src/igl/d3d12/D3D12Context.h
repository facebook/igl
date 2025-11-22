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

/**
 * @brief Descriptor heap page for dynamic multi-page growth
 *
 * Part of Strategy 1 (Transient Descriptor Allocator) architecture.
 * See D3D12ResourcesBinder.h for full architecture documentation.
 *
 * Following Microsoft MiniEngine's DynamicDescriptorHeap pattern:
 * - Start with 1 page of 1024 descriptors per frame
 * - Grow to up to 16 pages (16,384 descriptors) on-demand
 * - Reset all counters at frame boundary (no deallocation needed)
 */
struct DescriptorHeapPage {
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> heap;
  uint32_t capacity;  // Total descriptors in this page
  uint32_t used;      // Currently allocated descriptors

  DescriptorHeapPage() : capacity(0), used(0) {}
  DescriptorHeapPage(igl::d3d12::ComPtr<ID3D12DescriptorHeap> h, uint32_t cap)
      : heap(h), capacity(cap), used(0) {}
};

/**
 * @brief Per-frame context for CPU/GPU parallelism and descriptor management
 *
 * ============================================================================
 * ARCHITECTURE: Strategy 1 - Transient Descriptor Allocator
 * ============================================================================
 *
 * FrameContext implements the per-frame descriptor heap management system
 * (Strategy 1 in D3D12ResourcesBinder.h architecture).
 *
 * **Key Design Decisions**:
 * - 3 frames in flight: Prevents CPU/GPU stalls while enabling triple buffering
 * - Per-frame isolation: Each frame gets independent descriptor heaps
 * - Shared across command buffers: ALL command buffers in a frame share these heaps
 * - Linear allocation: O(1) descriptor allocation with simple counter increment
 * - Frame-boundary reset: Counters reset to 0, no per-descriptor deallocation
 * - Dynamic growth: CBV/SRV/UAV heaps can grow from 1 to 16 pages on-demand
 *
 * **Descriptor Heap Layout**:
 * - CBV/SRV/UAV: Multi-page array (1024 descriptors/page, up to 16 pages = 16K total)
 * - Samplers: Single heap (2048 descriptors, D3D12 spec limit, no growth)
 *
 * **Access Pattern**:
 * - CommandBuffer::getNextCbvSrvUavDescriptor() - allocates from current page
 * - CommandBuffer::allocateCbvSrvUavRange() - allocates contiguous range
 * - CommandBuffer::getNextSamplerDescriptor() - returns reference for increment
 * - FrameManager::resetDescriptorCounters() - resets at frame boundary
 *
 * **Performance Characteristics**:
 * - Allocation: O(1) with occasional page growth (O(n) for page vector resize)
 * - Deallocation: None (bulk reset at frame boundary)
 * - Memory: ~4MB worst case per frame (16 pages * 1024 descriptors * 32 bytes/descriptor)
 *
 * For architecture overview, see D3D12ResourcesBinder.h documentation.
 */
struct FrameContext {
  igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator;
  UINT64 fenceValue = 0;  // First fence signaled this frame (backward compatibility)

  // D-002: Track maximum fence value of ALL command lists using this allocator
  // CRITICAL: Allocator can only be reset when GPU completes maxAllocatorFence
  UINT64 maxAllocatorFence = 0;

  // D-002: Count command buffers submitted with this allocator (telemetry)
  uint32_t commandBufferCount = 0;

  // Per-frame shader-visible descriptor heaps (following Microsoft MiniEngine pattern).
  // Supports multiple pages for dynamic growth to prevent overflow and corruption.
  // Each frame gets its own isolated heap pages to prevent descriptor conflicts.
  std::vector<DescriptorHeapPage> cbvSrvUavHeapPages;  // Dynamic array of 1024-descriptor pages
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> samplerHeap;     // 2048 descriptors (kMaxSamplers)

  // Current active page index for CBV/SRV/UAV allocation.
  uint32_t currentCbvSrvUavPageIndex = 0;

  // Track the currently active shader-visible heap for command list binding.
  // This is updated when allocating new pages and must be rebound to the command list.
  // This heap is returned by D3D12Context::getCbvSrvUavHeap() for binding.
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> activeCbvSrvUavHeap;

  // Linear allocator counters - reset to 0 each frame
  // Incremented by each command buffer's encoders as they allocate descriptors
  uint32_t nextCbvSrvUavDescriptor = 0;
  uint32_t nextSamplerDescriptor = 0;

  // Transient resources that must be kept alive until this frame completes GPU execution
  // Examples: push constant buffers, temporary upload buffers
  // CRITICAL: These are cleared when we advance to the next frame AFTER waiting for
  // this frame's fence, ensuring the GPU has finished reading them
  std::vector<std::shared_ptr<igl::IBuffer>> transientBuffers;
  std::vector<igl::d3d12::ComPtr<ID3D12Resource>> transientResources;

  // Telemetry for transient resource tracking.
  // Tracks high-water mark to observe peak usage and detect unbounded growth.
  size_t transientBuffersHighWater = 0;
  size_t transientResourcesHighWater = 0;

  // Telemetry for descriptor heap usage tracking.
  // Tracks peak descriptor usage per frame to detect heap overflow risks.
  uint32_t peakCbvSrvUavUsage = 0;
  uint32_t peakSamplerUsage = 0;
};

class D3D12Context {
 public:
  // A-011: Multi-adapter enumeration and tracking
  struct AdapterInfo {
    igl::d3d12::ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 desc;
    D3D_FEATURE_LEVEL featureLevel;
    bool isWarp;  // Software rasterizer
    uint32_t index;  // Original enumeration index

    // Helper methods
    uint64_t getDedicatedVideoMemoryMB() const {
      return desc.DedicatedVideoMemory / (1024 * 1024);
    }

    const char* getVendorName() const {
      switch (desc.VendorId) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: case 0x1022: return "AMD";
        case 0x8086: return "Intel";
        case 0x1414: return "Microsoft";
        default: return "Unknown";
      }
    }
  };

  // A-012: Memory budget tracking
  struct MemoryBudget {
    uint64_t dedicatedVideoMemory = 0;  // Dedicated GPU memory (bytes)
    uint64_t sharedSystemMemory = 0;     // Shared system memory accessible to GPU (bytes)
    uint64_t estimatedUsage = 0;         // Current estimated usage by this device (bytes)
    uint64_t userDefinedBudgetLimit = 0; // Optional soft limit

    uint64_t totalAvailableMemory() const {
      return dedicatedVideoMemory + sharedSystemMemory;
    }

    double getUsagePercentage() const {
      if (totalAvailableMemory() == 0) return 0.0;
      return (static_cast<double>(estimatedUsage) / totalAvailableMemory()) * 100.0;
    }

    bool isMemoryCritical() const {
      return getUsagePercentage() > 90.0;
    }

    bool isMemoryLow() const {
      return getUsagePercentage() > 70.0;
    }
  };

  // A-010: HDR output capabilities
  struct HDRCapabilities {
    bool hdrSupported = false;           // HDR10 support
    bool scRGBSupported = false;          // scRGB (FP16) support
    DXGI_COLOR_SPACE_TYPE nativeColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;  // SDR default
    float maxLuminance = 80.0f;          // Max luminance in nits (SDR default)
    float minLuminance = 0.0f;           // Min luminance in nits
    float maxFullFrameLuminance = 80.0f; // Max full-frame luminance in nits
  };

  D3D12Context() = default;
  ~D3D12Context();

  // initialize() accepts optional D3D12ContextConfig for configurable sizes.
  Result initialize(HWND hwnd, uint32_t width, uint32_t height,
                   const D3D12ContextConfig& config = D3D12ContextConfig::defaultConfig());
  Result resize(uint32_t width, uint32_t height);

  ID3D12Device* getDevice() const { return device_.Get(); }
  ID3D12CommandQueue* getCommandQueue() const { return commandQueue_.Get(); }
  IDXGISwapChain3* getSwapChain() const { return swapChain_.Get(); }

  // Get currently active CBV/SRV/UAV descriptor heap for current frame.
  // Returns the active heap used for descriptor allocation. Use this for heap binding.
  // For multi-page access or diagnostics, use getFrameContexts().
  ID3D12DescriptorHeap* getCbvSrvUavHeap() const {
    const auto& frameCtx = frameContexts_[currentFrameIndex_];
    return frameCtx.activeCbvSrvUavHeap.Get();
  }
  ID3D12DescriptorHeap* getSamplerHeap() const {
    return frameContexts_[currentFrameIndex_].samplerHeap.Get();
  }

  // Allocate a new descriptor heap page for dynamic growth.
  Result allocateDescriptorHeapPage(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                     uint32_t numDescriptors,
                                     igl::d3d12::ComPtr<ID3D12DescriptorHeap>* outHeap);

  // Get descriptor sizes
  UINT getCbvSrvUavDescriptorSize() const { return cbvSrvUavDescriptorSize_; }
  UINT getSamplerDescriptorSize() const { return samplerDescriptorSize_; }

  // Get root signature capabilities
  D3D_ROOT_SIGNATURE_VERSION getHighestRootSignatureVersion() const { return highestRootSignatureVersion_; }
  D3D12_RESOURCE_BINDING_TIER getResourceBindingTier() const { return resourceBindingTier_; }

  // Get shader model capability.
  D3D_SHADER_MODEL getMaxShaderModel() const { return maxShaderModel_; }

  // Get selected feature level (A-004, A-005)
  D3D_FEATURE_LEVEL getSelectedFeatureLevel() const { return selectedFeatureLevel_; }

  // Get tearing support capability
  bool isTearingSupported() const { return tearingSupported_; }

  // Get command signatures for indirect drawing.
  ID3D12CommandSignature* getDrawIndirectSignature() const { return drawIndirectSignature_.Get(); }
  ID3D12CommandSignature* getDrawIndexedIndirectSignature() const { return drawIndexedIndirectSignature_.Get(); }

  // Get descriptor handles from per-frame heaps using the current page for multi-heap support.
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
  FrameContext* getFrameContexts() { return frameContexts_.data(); }
  UINT& getCurrentFrameIndex() { return currentFrameIndex_; }
  UINT getSwapchainBufferCount() const { return swapchainBufferCount_; }
  UINT64& getFenceValue() { return fenceValue_; }
  ID3D12Fence* getFence() const { return fence_.Get(); }

  // Resource tracking for diagnostics
  static void trackResourceCreation(const char* type, size_t sizeBytes);
  static void trackResourceDestruction(const char* type, size_t sizeBytes);
  static void logResourceStats();

  // A-011: Adapter enumeration and selection
  const std::vector<AdapterInfo>& getEnumeratedAdapters() const { return enumeratedAdapters_; }
  const AdapterInfo* getSelectedAdapter() const {
    if (selectedAdapterIndex_ < enumeratedAdapters_.size()) {
      return &enumeratedAdapters_[selectedAdapterIndex_];
    }
    return nullptr;
  }
  uint32_t getSelectedAdapterIndex() const { return selectedAdapterIndex_; }

  // A-012: Memory budget tracking
  MemoryBudget getMemoryBudget() const {
    std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
    return memoryBudget_;
  }

  double getMemoryUsagePercentage() const {
    std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
    return memoryBudget_.getUsagePercentage();
  }

  bool isMemoryLow() const {
    std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
    return memoryBudget_.isMemoryLow();
  }

  bool isMemoryCritical() const {
    std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
    return memoryBudget_.isMemoryCritical();
  }

  void updateMemoryUsage(int64_t delta) {
    std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
    uint64_t newUsage = memoryBudget_.estimatedUsage;
    if (delta < 0) {
      uint64_t absDelta = static_cast<uint64_t>(-delta);
      newUsage = (absDelta > newUsage) ? 0 : (newUsage - absDelta);
    } else {
      newUsage += static_cast<uint64_t>(delta);
    }
    memoryBudget_.estimatedUsage = newUsage;
  }

  // A-010: HDR output capabilities
  const HDRCapabilities& getHDRCapabilities() const { return hdrCapabilities_; }
  bool isHDRSupported() const { return hdrCapabilities_.hdrSupported; }

 protected:
  [[nodiscard]] Result createDevice();
  [[nodiscard]] Result createCommandQueue();
  [[nodiscard]] Result createSwapChain(HWND hwnd, uint32_t width, uint32_t height);
  Result recreateSwapChain(uint32_t width, uint32_t height);
  [[nodiscard]] Result createRTVHeap();
  [[nodiscard]] Result createBackBuffers();
  [[nodiscard]] Result createDescriptorHeaps();
  [[nodiscard]] Result createCommandSignatures();

  // A-011: Adapter enumeration
  [[nodiscard]] Result enumerateAndSelectAdapter();
  static D3D_FEATURE_LEVEL getHighestFeatureLevel(IDXGIAdapter1* adapter);

  // A-012: Memory budget detection
  void detectMemoryBudget();

  // A-010: HDR output detection
  void detectHDRCapabilities();

  igl::d3d12::ComPtr<IDXGIFactory4> dxgiFactory_;
  igl::d3d12::ComPtr<IDXGIAdapter1> adapter_;
  igl::d3d12::ComPtr<ID3D12Device> device_;
  igl::d3d12::ComPtr<ID3D12CommandQueue> commandQueue_;
  igl::d3d12::ComPtr<IDXGISwapChain3> swapChain_;
  UINT swapchainBufferCount_ = 0;  // Queried from swapchain, replaces kMaxFramesInFlight

  igl::d3d12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  std::vector<igl::d3d12::ComPtr<ID3D12Resource>> renderTargets_;  // Sized to swapchainBufferCount_
  UINT rtvDescriptorSize_ = 0;

  // Descriptor sizes (cached from device)
  UINT cbvSrvUavDescriptorSize_ = 0;
  UINT samplerDescriptorSize_ = 0;

  // Feature detection for root signature capabilities
  D3D_ROOT_SIGNATURE_VERSION highestRootSignatureVersion_ = D3D_ROOT_SIGNATURE_VERSION_1_0;
  D3D12_RESOURCE_BINDING_TIER resourceBindingTier_ = D3D12_RESOURCE_BINDING_TIER_1;

  // Feature detection for device feature level (A-004)
  D3D_FEATURE_LEVEL selectedFeatureLevel_ = D3D_FEATURE_LEVEL_11_0;

  // Feature detection for shader model.
  // DXC requires SM 6.0 minimum (SM 5.x deprecated).
  D3D_SHADER_MODEL maxShaderModel_ = D3D_SHADER_MODEL_6_0;

  // Feature detection for variable refresh rate (tearing) support
  bool tearingSupported_ = false;

  // A-011: Multi-adapter tracking (structs defined in public section)
  std::vector<AdapterInfo> enumeratedAdapters_;
  uint32_t selectedAdapterIndex_ = 0;

  // A-012: Memory budget tracking (struct defined in public section)
  MemoryBudget memoryBudget_;
  mutable std::mutex memoryTrackingMutex_;

  // A-010: HDR output capabilities (struct defined in public section)
  HDRCapabilities hdrCapabilities_;

  // Command signatures for indirect drawing.
  igl::d3d12::ComPtr<ID3D12CommandSignature> drawIndirectSignature_;
  igl::d3d12::ComPtr<ID3D12CommandSignature> drawIndexedIndirectSignature_;

  // Descriptor heap manager for headless contexts (unit tests)
  DescriptorHeapManager* ownedHeapMgr_ = nullptr;  // Owned manager for windowed contexts (raw ptr, manually deleted)
  DescriptorHeapManager* heapMgr_ = nullptr; // non-owning; points to ownedHeapMgr_ or external (headless)

  // Per-frame synchronization for CPU/GPU parallelism
  std::vector<FrameContext> frameContexts_;  // Sized to swapchainBufferCount_
  UINT currentFrameIndex_ = 0;

  // Global synchronization
  igl::d3d12::ComPtr<ID3D12Fence> fence_;
  UINT64 fenceValue_ = 0;

  uint32_t width_ = 0;
  uint32_t height_ = 0;

  // Configuration for customizable sizes.
  D3D12ContextConfig config_;

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
