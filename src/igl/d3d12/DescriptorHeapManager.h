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

// Simple descriptor heap manager suitable for unit tests and headless runs.
class DescriptorHeapManager {
 public:
  struct Sizes {
    uint32_t cbvSrvUav = 256; // shader-visible
    uint32_t samplers = 16;    // shader-visible
    uint32_t rtvs = 64;        // CPU-visible
    uint32_t dsvs = 32;        // CPU-visible
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

  // Get CPU/GPU handles for the given indices
  D3D12_CPU_DESCRIPTOR_HANDLE getRTVHandle(uint32_t index) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getDSVHandle(uint32_t index) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getCbvSrvUavCpuHandle(uint32_t index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE getCbvSrvUavGpuHandle(uint32_t index) const;
  D3D12_CPU_DESCRIPTOR_HANDLE getSamplerCpuHandle(uint32_t index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE getSamplerGpuHandle(uint32_t index) const;

  uint32_t getCbvSrvUavDescriptorSize() const { return cbvSrvUavDescriptorSize_; }
  uint32_t getSamplerDescriptorSize() const { return samplerDescriptorSize_; }
  uint32_t getRtvDescriptorSize() const { return rtvDescriptorSize_; }
  uint32_t getDsvDescriptorSize() const { return dsvDescriptorSize_; }

  // Telemetry: Log current heap usage statistics
  void logUsageStats() const;

 private:
  // Heaps
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;

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

  // Total sizes
  Sizes sizes_{};

  // High-watermark tracking
  uint32_t highWaterMarkCbvSrvUav_ = 0;
  uint32_t highWaterMarkSamplers_ = 0;
  uint32_t highWaterMarkRtvs_ = 0;
  uint32_t highWaterMarkDsvs_ = 0;

  // Thread safety
  mutable std::mutex mutex_;
};

} // namespace igl::d3d12
