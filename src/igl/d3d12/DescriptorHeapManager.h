/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <vector>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

/**
 * @brief Persistent Descriptor Allocator for CPU-visible and long-lived descriptors
 *
 * ============================================================================
 * ARCHITECTURE: Strategy 2 - Persistent Descriptor Allocator
 * ============================================================================
 *
 * DescriptorHeapManager handles descriptors with EXPLICIT lifecycle management:
 * - **PRIMARY USE**: CPU-visible descriptors (RTV/DSV) for Texture and Framebuffer
 * - **SECONDARY USE**: Shader-visible descriptors for headless/unit test contexts
 *
 * **Key Differences from Per-Frame System (Strategy 1)**:
 * - Lifecycle: Allocated at resource creation, freed at destruction (not per-frame reset)
 * - Allocation: Free-list pattern (not linear) - supports arbitrary alloc/free
 * - Safety: Double-free detection, mutex protection for thread-safety
 * - Visibility: Creates both CPU-visible AND shader-visible heaps
 *
 * **When to Use This vs Per-Frame (D3D12ResourcesBinder)**:
 * - Use DescriptorHeapManager for: RTV/DSV allocation for textures/framebuffers
 * - Use DescriptorHeapManager for: Headless contexts without per-frame infrastructure
 * - Do NOT use for: Transient SRV/UAV/CBV/Samplers during rendering
 * - Do NOT use for: Descriptor table binding in encoders
 *
 * **Design Note**: This class creates shader-visible heaps (CBV/SRV/UAV, Samplers)
 * for backward compatibility with headless contexts. In normal rendering contexts:
 * - D3D12Context uses per-frame heaps (Strategy 1) for shader-visible descriptors
 * - DescriptorHeapManager is only used for RTV/DSV allocation
 * - Its shader-visible heaps serve as a fallback when per-frame heaps are unavailable
 *   (e.g., headless/unit-test contexts - see ComputeCommandEncoder.cpp:32-40)
 *
 * For architecture overview, see D3D12ResourcesBinder.h documentation.
 *
 * Thread-safety: This class IS thread-safe (uses mutex for allocation/free).
 */
class DescriptorHeapManager {
 public:
  // Descriptor heap sizes configuration.
  // Default values match D3D12ContextConfig for consistency but can be customized at runtime.
  struct Sizes {
    uint32_t cbvSrvUav = 4096; // shader-visible (kept larger for unit tests/headless)
    uint32_t samplers = 2048;  // shader-visible (D3D12 spec limit)
    uint32_t rtvs = 256;       // CPU-visible (default from D3D12ContextConfig)
    uint32_t dsvs = 128;       // CPU-visible (default from D3D12ContextConfig)

    // Note: D3D12Context and HeadlessContext construct Sizes manually based on their
    // specific needs (environment overrides, test requirements, etc.) rather than using
    // a generic factory method. To customize, construct Sizes with desired values.
  };

  DescriptorHeapManager() = default;
  Result initialize(ID3D12Device* device, const Sizes& sizes = {});

  // Shader-visible heaps for binding
  ID3D12DescriptorHeap* getCbvSrvUavHeap() const { return cbvSrvUavHeap_.Get(); }
  ID3D12DescriptorHeap* getSamplerHeap() const { return samplerHeap_.Get(); }

  // Allocate a CPU descriptor from RTV/DSV heaps
  uint32_t allocateRTV();
  uint32_t allocateDSV();
  void freeRTV(uint32_t index);
  void freeDSV(uint32_t index);

  // Allocate indices inside shader-visible heaps (for creating CBV/SRV/UAV or Samplers)
  uint32_t allocateCbvSrvUav();
  uint32_t allocateSampler();
  void freeCbvSrvUav(uint32_t index);
  void freeSampler(uint32_t index);

  // Get CPU/GPU handles for the given indices (with validation).
  D3D12_CPU_DESCRIPTOR_HANDLE getRTVHandle(uint32_t index) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getDSVHandle(uint32_t index) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getCbvSrvUavCpuHandle(uint32_t index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE getCbvSrvUavGpuHandle(uint32_t index) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getSamplerCpuHandle(uint32_t index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE getSamplerGpuHandle(uint32_t index) const;

  // Alternative error-checking variants with bool return.
  // Returns false on error (invalid index, null heap), true on success.
  [[nodiscard]] bool getRTVHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE* outHandle) const;
  [[nodiscard]] bool getDSVHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE* outHandle) const;
  [[nodiscard]] bool getCbvSrvUavCpuHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE* outHandle) const;
  [[nodiscard]] bool getCbvSrvUavGpuHandle(uint32_t index, D3D12_GPU_DESCRIPTOR_HANDLE* outHandle) const;
  [[nodiscard]] bool getSamplerCpuHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE* outHandle) const;
  [[nodiscard]] bool getSamplerGpuHandle(uint32_t index, D3D12_GPU_DESCRIPTOR_HANDLE* outHandle) const;

  uint32_t getCbvSrvUavDescriptorSize() const { return cbvSrvUavDescriptorSize_; }
  uint32_t getSamplerDescriptorSize() const { return samplerDescriptorSize_; }
  uint32_t getRtvDescriptorSize() const { return rtvDescriptorSize_; }
  uint32_t getDsvDescriptorSize() const { return dsvDescriptorSize_; }

  // Descriptor handle validation helpers.
  [[nodiscard]] bool isValidRTVIndex(uint32_t index) const;
  [[nodiscard]] bool isValidDSVIndex(uint32_t index) const;
  [[nodiscard]] bool isValidCbvSrvUavIndex(uint32_t index) const;
  [[nodiscard]] bool isValidSamplerIndex(uint32_t index) const;

  // Telemetry: Log current heap usage statistics
  void logUsageStats() const;

  // Explicit cleanup of descriptor heaps to prevent leaks.
  void cleanup();

 private:
  // Heaps
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> samplerHeap_;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> dsvHeap_;

  // Increments
  UINT cbvSrvUavDescriptorSize_ = 0;
  UINT samplerDescriptorSize_ = 0;
  UINT rtvDescriptorSize_ = 0;
  UINT dsvDescriptorSize_ = 0;

  // Free lists for CPU-only heaps
  std::vector<uint32_t> freeRtvs_;
  std::vector<uint32_t> freeDsvs_;
  // Free lists for shader-visible heaps
  std::vector<uint32_t> freeCbvSrvUav_;
  std::vector<uint32_t> freeSamplers_;

  // Allocation state tracking (prevents double-free)
  std::vector<bool> allocatedRtvs_;
  std::vector<bool> allocatedDsvs_;
  std::vector<bool> allocatedCbvSrvUav_;
  std::vector<bool> allocatedSamplers_;

  // Total sizes
  Sizes sizes_{};

  // Thread safety
  mutable std::mutex mutex_;

  // A-006: Validate and clamp descriptor heap sizes to device limits
  void validateAndClampSizes(ID3D12Device* device);
};

} // namespace igl::d3d12
