/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/DescriptorHeapManager.h>

namespace igl::d3d12 {

Result DescriptorHeapManager::initialize(ID3D12Device* device, const Sizes& sizes) {
  if (!device) {
    return Result(Result::Code::ArgumentInvalid, "Null device for DescriptorHeapManager");
  }
  sizes_ = sizes;

  // Create shader-visible CBV/SRV/UAV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = sizes_.cbvSrvUav;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(cbvSrvUavHeap_.GetAddressOf())))) {
      return Result(Result::Code::RuntimeError, "Failed to create CBV/SRV/UAV heap");
    }
    cbvSrvUavDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // Populate free list
    freeCbvSrvUav_.reserve(sizes_.cbvSrvUav);
    for (uint32_t i = 0; i < sizes_.cbvSrvUav; ++i) {
      freeCbvSrvUav_.push_back(i);
    }
  }

  // Create shader-visible sampler heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    desc.NumDescriptors = sizes_.samplers;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(samplerHeap_.GetAddressOf())))) {
      return Result(Result::Code::RuntimeError, "Failed to create sampler heap");
    }
    samplerDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    // Populate free list
    freeSamplers_.reserve(sizes_.samplers);
    for (uint32_t i = 0; i < sizes_.samplers; ++i) {
      freeSamplers_.push_back(i);
    }
  }

  // Create CPU-visible RTV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = sizes_.rtvs;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap_.GetAddressOf())))) {
      return Result(Result::Code::RuntimeError, "Failed to create RTV heap");
    }
    rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // Populate free list
    freeRtvs_.reserve(sizes_.rtvs);
    for (uint32_t i = 0; i < sizes_.rtvs; ++i) {
      freeRtvs_.push_back(i);
    }
  }

  // Create CPU-visible DSV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    desc.NumDescriptors = sizes_.dsvs;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(dsvHeap_.GetAddressOf())))) {
      return Result(Result::Code::RuntimeError, "Failed to create DSV heap");
    }
    dsvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // Populate free list
    freeDsvs_.reserve(sizes_.dsvs);
    for (uint32_t i = 0; i < sizes_.dsvs; ++i) {
      freeDsvs_.push_back(i);
    }
  }

  return Result();
}

uint32_t DescriptorHeapManager::allocateRTV() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (freeRtvs_.empty()) {
    IGL_LOG_ERROR("DescriptorHeapManager: RTV heap exhausted! "
                  "Requested allocation failed (capacity: %u descriptors)\n",
                  sizes_.rtvs);
    return UINT32_MAX;
  }
  const uint32_t idx = freeRtvs_.back();
  freeRtvs_.pop_back();

  // Track high-watermark
  const uint32_t currentUsage = sizes_.rtvs - static_cast<uint32_t>(freeRtvs_.size());
  if (currentUsage > highWaterMarkRtvs_) {
    highWaterMarkRtvs_ = currentUsage;
  }

  return idx;
}

uint32_t DescriptorHeapManager::allocateDSV() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (freeDsvs_.empty()) {
    IGL_LOG_ERROR("DescriptorHeapManager: DSV heap exhausted! "
                  "Requested allocation failed (capacity: %u descriptors)\n",
                  sizes_.dsvs);
    return UINT32_MAX;
  }
  const uint32_t idx = freeDsvs_.back();
  freeDsvs_.pop_back();

  // Track high-watermark
  const uint32_t currentUsage = sizes_.dsvs - static_cast<uint32_t>(freeDsvs_.size());
  if (currentUsage > highWaterMarkDsvs_) {
    highWaterMarkDsvs_ = currentUsage;
  }

  return idx;
}

void DescriptorHeapManager::freeRTV(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.rtvs) {
    freeRtvs_.push_back(index);
  }
}

void DescriptorHeapManager::freeDSV(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.dsvs) {
    freeDsvs_.push_back(index);
  }
}

uint32_t DescriptorHeapManager::allocateCbvSrvUav() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (freeCbvSrvUav_.empty()) {
    IGL_LOG_ERROR("DescriptorHeapManager: CBV/SRV/UAV heap exhausted! "
                  "Requested allocation failed (capacity: %u descriptors)\n",
                  sizes_.cbvSrvUav);
    return UINT32_MAX;
  }
  const uint32_t idx = freeCbvSrvUav_.back();
  freeCbvSrvUav_.pop_back();

  // Track high-watermark
  const uint32_t currentUsage = sizes_.cbvSrvUav - static_cast<uint32_t>(freeCbvSrvUav_.size());
  if (currentUsage > highWaterMarkCbvSrvUav_) {
    highWaterMarkCbvSrvUav_ = currentUsage;
  }

  return idx;
}

uint32_t DescriptorHeapManager::allocateSampler() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (freeSamplers_.empty()) {
    IGL_LOG_ERROR("DescriptorHeapManager: Sampler heap exhausted! "
                  "Requested allocation failed (capacity: %u descriptors)\n",
                  sizes_.samplers);
    return UINT32_MAX;
  }
  const uint32_t idx = freeSamplers_.back();
  freeSamplers_.pop_back();

  // Track high-watermark
  const uint32_t currentUsage = sizes_.samplers - static_cast<uint32_t>(freeSamplers_.size());
  if (currentUsage > highWaterMarkSamplers_) {
    highWaterMarkSamplers_ = currentUsage;
  }

  return idx;
}

void DescriptorHeapManager::freeCbvSrvUav(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.cbvSrvUav) {
    freeCbvSrvUav_.push_back(index);
  }
}

void DescriptorHeapManager::freeSampler(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.samplers) {
    freeSamplers_.push_back(index);
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getRTVHandle(uint32_t index) const {
  D3D12_CPU_DESCRIPTOR_HANDLE h = {};
  if (!rtvHeap_.Get() || index >= sizes_.rtvs) {
    return h;
  }
  h = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
  h.ptr += index * rtvDescriptorSize_;
  return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getDSVHandle(uint32_t index) const {
  D3D12_CPU_DESCRIPTOR_HANDLE h = {};
  if (!dsvHeap_.Get() || index >= sizes_.dsvs) {
    return h;
  }
  h = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  h.ptr += index * dsvDescriptorSize_;
  return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCbvSrvUavCpuHandle(uint32_t index) const {
  D3D12_CPU_DESCRIPTOR_HANDLE h = {};
  if (!cbvSrvUavHeap_.Get() || index >= sizes_.cbvSrvUav) {
    return h;
  }
  h = cbvSrvUavHeap_->GetCPUDescriptorHandleForHeapStart();
  h.ptr += index * cbvSrvUavDescriptorSize_;
  return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCbvSrvUavGpuHandle(uint32_t index) const {
  D3D12_GPU_DESCRIPTOR_HANDLE h = {};
  if (!cbvSrvUavHeap_.Get() || index >= sizes_.cbvSrvUav) {
    return h;
  }
  h = cbvSrvUavHeap_->GetGPUDescriptorHandleForHeapStart();
  h.ptr += index * cbvSrvUavDescriptorSize_;
  return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getSamplerCpuHandle(uint32_t index) const {
  D3D12_CPU_DESCRIPTOR_HANDLE h = {};
  if (!samplerHeap_.Get() || index >= sizes_.samplers) {
    return h;
  }
  h = samplerHeap_->GetCPUDescriptorHandleForHeapStart();
  h.ptr += index * samplerDescriptorSize_;
  return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getSamplerGpuHandle(uint32_t index) const {
  D3D12_GPU_DESCRIPTOR_HANDLE h = {};
  if (!samplerHeap_.Get() || index >= sizes_.samplers) {
    return h;
  }
  h = samplerHeap_->GetGPUDescriptorHandleForHeapStart();
  h.ptr += index * samplerDescriptorSize_;
  return h;
}

void DescriptorHeapManager::logUsageStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  IGL_LOG_INFO("=== Descriptor Heap Usage Statistics ===\n");

  // CBV/SRV/UAV heap
  const uint32_t cbvSrvUavUsed = sizes_.cbvSrvUav - static_cast<uint32_t>(freeCbvSrvUav_.size());
  const float cbvSrvUavPercent = (cbvSrvUavUsed * 100.0f) / sizes_.cbvSrvUav;
  IGL_LOG_INFO("  CBV/SRV/UAV: %u / %u (%.1f%% used)\n",
               cbvSrvUavUsed, sizes_.cbvSrvUav, cbvSrvUavPercent);

  // Sampler heap
  const uint32_t samplersUsed = sizes_.samplers - static_cast<uint32_t>(freeSamplers_.size());
  const float samplersPercent = (samplersUsed * 100.0f) / sizes_.samplers;
  IGL_LOG_INFO("  Samplers:    %u / %u (%.1f%% used)\n",
               samplersUsed, sizes_.samplers, samplersPercent);

  // RTV heap
  const uint32_t rtvsUsed = sizes_.rtvs - static_cast<uint32_t>(freeRtvs_.size());
  const float rtvsPercent = (rtvsUsed * 100.0f) / sizes_.rtvs;
  IGL_LOG_INFO("  RTVs:        %u / %u (%.1f%% used)\n",
               rtvsUsed, sizes_.rtvs, rtvsPercent);

  // DSV heap
  const uint32_t dsvsUsed = sizes_.dsvs - static_cast<uint32_t>(freeDsvs_.size());
  const float dsvsPercent = (dsvsUsed * 100.0f) / sizes_.dsvs;
  IGL_LOG_INFO("  DSVs:        %u / %u (%.1f%% used)\n",
               dsvsUsed, sizes_.dsvs, dsvsPercent);

  IGL_LOG_INFO("\n");
  IGL_LOG_INFO("=== Peak Usage (High-Watermarks) ===\n");

  // Peak CBV/SRV/UAV
  const float cbvSrvUavPeakPercent = (highWaterMarkCbvSrvUav_ * 100.0f) / sizes_.cbvSrvUav;
  IGL_LOG_INFO("  Peak CBV/SRV/UAV: %u / %u (%.1f%% peak)\n",
               highWaterMarkCbvSrvUav_, sizes_.cbvSrvUav, cbvSrvUavPeakPercent);

  // Peak Samplers
  const float samplersPeakPercent = (highWaterMarkSamplers_ * 100.0f) / sizes_.samplers;
  IGL_LOG_INFO("  Peak Samplers:    %u / %u (%.1f%% peak)\n",
               highWaterMarkSamplers_, sizes_.samplers, samplersPeakPercent);

  // Peak RTVs
  const float rtvsPeakPercent = (highWaterMarkRtvs_ * 100.0f) / sizes_.rtvs;
  IGL_LOG_INFO("  Peak RTVs:        %u / %u (%.1f%% peak)\n",
               highWaterMarkRtvs_, sizes_.rtvs, rtvsPeakPercent);

  // Peak DSVs
  const float dsvsPeakPercent = (highWaterMarkDsvs_ * 100.0f) / sizes_.dsvs;
  IGL_LOG_INFO("  Peak DSVs:        %u / %u (%.1f%% peak)\n",
               highWaterMarkDsvs_, sizes_.dsvs, dsvsPeakPercent);

  IGL_LOG_INFO("========================================\n");
}

} // namespace igl::d3d12
