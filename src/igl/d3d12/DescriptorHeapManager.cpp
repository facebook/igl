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

  // A-006: Copy requested sizes, then validate/clamp against device limits
  sizes_ = sizes;
  validateAndClampSizes(device);

  // Create shader-visible CBV/SRV/UAV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = sizes_.cbvSrvUav;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(cbvSrvUavHeap_.GetAddressOf())))) {
      // A-006: Enhanced error message with size context
      IGL_LOG_ERROR("DescriptorHeapManager: Failed to create CBV/SRV/UAV heap "
                    "(size=%u descriptors)\n", sizes_.cbvSrvUav);
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
      // A-006: Enhanced error message with size context
      IGL_LOG_ERROR("DescriptorHeapManager: Failed to create sampler heap "
                    "(size=%u descriptors, limit=2048)\n", sizes_.samplers);
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
      // A-006: Enhanced error message with size context
      IGL_LOG_ERROR("DescriptorHeapManager: Failed to create RTV heap "
                    "(size=%u descriptors)\n", sizes_.rtvs);
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
      // A-006: Enhanced error message with size context
      IGL_LOG_ERROR("DescriptorHeapManager: Failed to create DSV heap "
                    "(size=%u descriptors)\n", sizes_.dsvs);
      return Result(Result::Code::RuntimeError, "Failed to create DSV heap");
    }
    dsvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // Populate free list
    freeDsvs_.reserve(sizes_.dsvs);
    for (uint32_t i = 0; i < sizes_.dsvs; ++i) {
      freeDsvs_.push_back(i);
    }
  }

  // Initialize allocation tracking bitsets (all descriptors start as free)
  allocatedRtvs_.resize(sizes_.rtvs, false);
  allocatedDsvs_.resize(sizes_.dsvs, false);
  allocatedCbvSrvUav_.resize(sizes_.cbvSrvUav, false);
  allocatedSamplers_.resize(sizes_.samplers, false);

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

  // Mark as allocated
  IGL_DEBUG_ASSERT(!allocatedRtvs_[idx], "Free list contained allocated descriptor!");
  allocatedRtvs_[idx] = true;

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

  // Mark as allocated
  IGL_DEBUG_ASSERT(!allocatedDsvs_[idx], "Free list contained allocated descriptor!");
  allocatedDsvs_[idx] = true;

  // Track high-watermark
  const uint32_t currentUsage = sizes_.dsvs - static_cast<uint32_t>(freeDsvs_.size());
  if (currentUsage > highWaterMarkDsvs_) {
    highWaterMarkDsvs_ = currentUsage;
  }

  return idx;
}

void DescriptorHeapManager::freeRTV(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Validate bounds
  if (index == UINT32_MAX || index >= sizes_.rtvs) {
    return;
  }

  // CRITICAL: Detect double-free bugs
  if (!allocatedRtvs_[index]) {
    IGL_LOG_ERROR("DescriptorHeapManager: DOUBLE-FREE DETECTED - RTV index %u already freed!\n", index);
    IGL_DEBUG_ASSERT(false, "Double-free of RTV descriptor - caller bug detected");
    return;  // Prevent corruption even in release builds
  }

  // Mark as free and add to free list
  allocatedRtvs_[index] = false;
  freeRtvs_.push_back(index);
}

void DescriptorHeapManager::freeDSV(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Validate bounds
  if (index == UINT32_MAX || index >= sizes_.dsvs) {
    return;
  }

  // CRITICAL: Detect double-free bugs
  if (!allocatedDsvs_[index]) {
    IGL_LOG_ERROR("DescriptorHeapManager: DOUBLE-FREE DETECTED - DSV index %u already freed!\n", index);
    IGL_DEBUG_ASSERT(false, "Double-free of DSV descriptor - caller bug detected");
    return;  // Prevent corruption even in release builds
  }

  // Mark as free and add to free list
  allocatedDsvs_[index] = false;
  freeDsvs_.push_back(index);
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

  // Mark as allocated
  IGL_DEBUG_ASSERT(!allocatedCbvSrvUav_[idx], "Free list contained allocated descriptor!");
  allocatedCbvSrvUav_[idx] = true;

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

  // Mark as allocated
  IGL_DEBUG_ASSERT(!allocatedSamplers_[idx], "Free list contained allocated descriptor!");
  allocatedSamplers_[idx] = true;

  // Track high-watermark
  const uint32_t currentUsage = sizes_.samplers - static_cast<uint32_t>(freeSamplers_.size());
  if (currentUsage > highWaterMarkSamplers_) {
    highWaterMarkSamplers_ = currentUsage;
  }

  return idx;
}

void DescriptorHeapManager::freeCbvSrvUav(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Validate bounds
  if (index == UINT32_MAX || index >= sizes_.cbvSrvUav) {
    return;
  }

  // CRITICAL: Detect double-free bugs
  if (!allocatedCbvSrvUav_[index]) {
    IGL_LOG_ERROR("DescriptorHeapManager: DOUBLE-FREE DETECTED - CBV/SRV/UAV index %u already freed!\n", index);
    IGL_DEBUG_ASSERT(false, "Double-free of CBV/SRV/UAV descriptor - caller bug detected");
    return;  // Prevent corruption even in release builds
  }

  // Mark as free and add to free list
  allocatedCbvSrvUav_[index] = false;
  freeCbvSrvUav_.push_back(index);
}

void DescriptorHeapManager::freeSampler(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Validate bounds
  if (index == UINT32_MAX || index >= sizes_.samplers) {
    return;
  }

  // CRITICAL: Detect double-free bugs
  if (!allocatedSamplers_[index]) {
    IGL_LOG_ERROR("DescriptorHeapManager: DOUBLE-FREE DETECTED - Sampler index %u already freed!\n", index);
    IGL_DEBUG_ASSERT(false, "Double-free of Sampler descriptor - caller bug detected");
    return;  // Prevent corruption even in release builds
  }

  // Mark as free and add to free list
  allocatedSamplers_[index] = false;
  freeSamplers_.push_back(index);
}

// C-007: Explicit error checking with bool return (builds on C-006 validation)
bool DescriptorHeapManager::getRTVHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE* outHandle) const {
  if (!outHandle) {
    IGL_LOG_ERROR("DescriptorHeapManager::getRTVHandle: outHandle is null\n");
    return false;
  }

  // Initialize to zero in case of error
  *outHandle = {};

  if (!rtvHeap_.Get()) {
    IGL_LOG_ERROR("DescriptorHeapManager::getRTVHandle: RTV heap is null\n");
    IGL_DEBUG_ASSERT(false, "RTV heap is null");
    return false;
  }

  if (index == UINT32_MAX) {
    IGL_LOG_ERROR("DescriptorHeapManager::getRTVHandle: Invalid index UINT32_MAX (allocation failure sentinel)\n");
    IGL_DEBUG_ASSERT(false, "Attempted to get RTV handle with invalid index UINT32_MAX");
    return false;
  }

  if (index >= sizes_.rtvs) {
    IGL_LOG_ERROR("DescriptorHeapManager::getRTVHandle: Index %u exceeds heap size %u\n",
                  index, sizes_.rtvs);
    IGL_DEBUG_ASSERT(false, "RTV descriptor index out of bounds");
    return false;
  }

  // Check if descriptor has been freed (use-after-free detection)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!allocatedRtvs_[index]) {
      IGL_LOG_ERROR("DescriptorHeapManager::getRTVHandle: Descriptor index %u has been freed (use-after-free)\n", index);
      IGL_DEBUG_ASSERT(false, "Use-after-free: Accessing freed RTV descriptor");
      return false;
    }
  }

  *outHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
  outHandle->ptr += index * rtvDescriptorSize_;

  // Validate final handle is non-null
  IGL_DEBUG_ASSERT(outHandle->ptr != 0, "getRTVHandle returned null CPU descriptor handle");

  return true;
}

// C-007: Explicit error checking with bool return (builds on C-006 validation)
bool DescriptorHeapManager::getDSVHandle(uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE* outHandle) const {
  if (!outHandle) {
    IGL_LOG_ERROR("DescriptorHeapManager::getDSVHandle: outHandle is null\n");
    return false;
  }

  // Initialize to zero in case of error
  *outHandle = {};

  if (!dsvHeap_.Get()) {
    IGL_LOG_ERROR("DescriptorHeapManager::getDSVHandle: DSV heap is null\n");
    IGL_DEBUG_ASSERT(false, "DSV heap is null");
    return false;
  }

  if (index == UINT32_MAX) {
    IGL_LOG_ERROR("DescriptorHeapManager::getDSVHandle: Invalid index UINT32_MAX (allocation failure sentinel)\n");
    IGL_DEBUG_ASSERT(false, "Attempted to get DSV handle with invalid index UINT32_MAX");
    return false;
  }

  if (index >= sizes_.dsvs) {
    IGL_LOG_ERROR("DescriptorHeapManager::getDSVHandle: Index %u exceeds heap size %u\n",
                  index, sizes_.dsvs);
    IGL_DEBUG_ASSERT(false, "DSV descriptor index out of bounds");
    return false;
  }

  // Check if descriptor has been freed (use-after-free detection)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!allocatedDsvs_[index]) {
      IGL_LOG_ERROR("DescriptorHeapManager::getDSVHandle: Descriptor index %u has been freed (use-after-free)\n", index);
      IGL_DEBUG_ASSERT(false, "Use-after-free: Accessing freed DSV descriptor");
      return false;
    }
  }

  *outHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  outHandle->ptr += index * dsvDescriptorSize_;

  // Validate final handle is non-null
  IGL_DEBUG_ASSERT(outHandle->ptr != 0, "getDSVHandle returned null CPU descriptor handle");

  return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCbvSrvUavCpuHandle(uint32_t index) const {
  // C-006: Validate descriptor index before returning handle
  D3D12_CPU_DESCRIPTOR_HANDLE h = {};

  if (!cbvSrvUavHeap_.Get()) {
    IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavCpuHandle: CBV/SRV/UAV heap is null\n");
    IGL_DEBUG_ASSERT(false, "CBV/SRV/UAV heap is null");
    return h;
  }

  if (index == UINT32_MAX) {
    IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavCpuHandle: Invalid index UINT32_MAX (allocation failure sentinel)\n");
    IGL_DEBUG_ASSERT(false, "Attempted to get CBV/SRV/UAV handle with invalid index UINT32_MAX");
    return h;
  }

  if (index >= sizes_.cbvSrvUav) {
    IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavCpuHandle: Index %u exceeds heap size %u\n",
                  index, sizes_.cbvSrvUav);
    IGL_DEBUG_ASSERT(false, "CBV/SRV/UAV descriptor index out of bounds");
    return h;
  }

  // Check if descriptor has been freed (use-after-free detection)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!allocatedCbvSrvUav_[index]) {
      IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavCpuHandle: Descriptor index %u has been freed (use-after-free)\n", index);
      IGL_DEBUG_ASSERT(false, "Use-after-free: Accessing freed CBV/SRV/UAV descriptor");
      return h;
    }
  }

  h = cbvSrvUavHeap_->GetCPUDescriptorHandleForHeapStart();
  h.ptr += index * cbvSrvUavDescriptorSize_;

  // Validate final handle is non-null
  IGL_DEBUG_ASSERT(h.ptr != 0, "getCbvSrvUavCpuHandle returned null CPU descriptor handle");

  return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCbvSrvUavGpuHandle(uint32_t index) const {
  // C-006: Validate descriptor index before returning handle
  D3D12_GPU_DESCRIPTOR_HANDLE h = {};

  if (!cbvSrvUavHeap_.Get()) {
    IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavGpuHandle: CBV/SRV/UAV heap is null\n");
    IGL_DEBUG_ASSERT(false, "CBV/SRV/UAV heap is null");
    return h;
  }

  if (index == UINT32_MAX) {
    IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavGpuHandle: Invalid index UINT32_MAX (allocation failure sentinel)\n");
    IGL_DEBUG_ASSERT(false, "Attempted to get CBV/SRV/UAV GPU handle with invalid index UINT32_MAX");
    return h;
  }

  if (index >= sizes_.cbvSrvUav) {
    IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavGpuHandle: Index %u exceeds heap size %u\n",
                  index, sizes_.cbvSrvUav);
    IGL_DEBUG_ASSERT(false, "CBV/SRV/UAV descriptor index out of bounds");
    return h;
  }

  // Check if descriptor has been freed (use-after-free detection)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!allocatedCbvSrvUav_[index]) {
      IGL_LOG_ERROR("DescriptorHeapManager::getCbvSrvUavGpuHandle: Descriptor index %u has been freed (use-after-free)\n", index);
      IGL_DEBUG_ASSERT(false, "Use-after-free: Accessing freed CBV/SRV/UAV descriptor");
      return h;
    }
  }

  h = cbvSrvUavHeap_->GetGPUDescriptorHandleForHeapStart();
  h.ptr += index * cbvSrvUavDescriptorSize_;

  // Validate final handle is non-null
  IGL_DEBUG_ASSERT(h.ptr != 0, "getCbvSrvUavGpuHandle returned null GPU descriptor handle");

  return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getSamplerCpuHandle(uint32_t index) const {
  // C-006: Validate descriptor index before returning handle
  D3D12_CPU_DESCRIPTOR_HANDLE h = {};

  if (!samplerHeap_.Get()) {
    IGL_LOG_ERROR("DescriptorHeapManager::getSamplerCpuHandle: Sampler heap is null\n");
    IGL_DEBUG_ASSERT(false, "Sampler heap is null");
    return h;
  }

  if (index == UINT32_MAX) {
    IGL_LOG_ERROR("DescriptorHeapManager::getSamplerCpuHandle: Invalid index UINT32_MAX (allocation failure sentinel)\n");
    IGL_DEBUG_ASSERT(false, "Attempted to get Sampler handle with invalid index UINT32_MAX");
    return h;
  }

  if (index >= sizes_.samplers) {
    IGL_LOG_ERROR("DescriptorHeapManager::getSamplerCpuHandle: Index %u exceeds heap size %u\n",
                  index, sizes_.samplers);
    IGL_DEBUG_ASSERT(false, "Sampler descriptor index out of bounds");
    return h;
  }

  // Check if descriptor has been freed (use-after-free detection)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!allocatedSamplers_[index]) {
      IGL_LOG_ERROR("DescriptorHeapManager::getSamplerCpuHandle: Descriptor index %u has been freed (use-after-free)\n", index);
      IGL_DEBUG_ASSERT(false, "Use-after-free: Accessing freed Sampler descriptor");
      return h;
    }
  }

  h = samplerHeap_->GetCPUDescriptorHandleForHeapStart();
  h.ptr += index * samplerDescriptorSize_;

  // Validate final handle is non-null
  IGL_DEBUG_ASSERT(h.ptr != 0, "getSamplerCpuHandle returned null CPU descriptor handle");

  return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getSamplerGpuHandle(uint32_t index) const {
  // C-006: Validate descriptor index before returning handle
  D3D12_GPU_DESCRIPTOR_HANDLE h = {};

  if (!samplerHeap_.Get()) {
    IGL_LOG_ERROR("DescriptorHeapManager::getSamplerGpuHandle: Sampler heap is null\n");
    IGL_DEBUG_ASSERT(false, "Sampler heap is null");
    return h;
  }

  if (index == UINT32_MAX) {
    IGL_LOG_ERROR("DescriptorHeapManager::getSamplerGpuHandle: Invalid index UINT32_MAX (allocation failure sentinel)\n");
    IGL_DEBUG_ASSERT(false, "Attempted to get Sampler GPU handle with invalid index UINT32_MAX");
    return h;
  }

  if (index >= sizes_.samplers) {
    IGL_LOG_ERROR("DescriptorHeapManager::getSamplerGpuHandle: Index %u exceeds heap size %u\n",
                  index, sizes_.samplers);
    IGL_DEBUG_ASSERT(false, "Sampler descriptor index out of bounds");
    return h;
  }

  // Check if descriptor has been freed (use-after-free detection)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!allocatedSamplers_[index]) {
      IGL_LOG_ERROR("DescriptorHeapManager::getSamplerGpuHandle: Descriptor index %u has been freed (use-after-free)\n", index);
      IGL_DEBUG_ASSERT(false, "Use-after-free: Accessing freed Sampler descriptor");
      return h;
    }
  }

  h = samplerHeap_->GetGPUDescriptorHandleForHeapStart();
  h.ptr += index * samplerDescriptorSize_;

  // Validate final handle is non-null
  IGL_DEBUG_ASSERT(h.ptr != 0, "getSamplerGpuHandle returned null GPU descriptor handle");

  return h;
}

// C-006: Descriptor handle validation helpers
bool DescriptorHeapManager::isValidRTVIndex(uint32_t index) const {
  if (index == UINT32_MAX) {
    return false;  // Sentinel value for allocation failure
  }
  if (index >= sizes_.rtvs) {
    return false;  // Out of bounds
  }
  // Check if descriptor is currently allocated (not in free list)
  // This helps detect use-after-free bugs
  std::lock_guard<std::mutex> lock(mutex_);
  return allocatedRtvs_[index];
}

bool DescriptorHeapManager::isValidDSVIndex(uint32_t index) const {
  if (index == UINT32_MAX) {
    return false;  // Sentinel value for allocation failure
  }
  if (index >= sizes_.dsvs) {
    return false;  // Out of bounds
  }
  // Check if descriptor is currently allocated
  std::lock_guard<std::mutex> lock(mutex_);
  return allocatedDsvs_[index];
}

bool DescriptorHeapManager::isValidCbvSrvUavIndex(uint32_t index) const {
  if (index == UINT32_MAX) {
    return false;  // Sentinel value for allocation failure
  }
  if (index >= sizes_.cbvSrvUav) {
    return false;  // Out of bounds
  }
  // Check if descriptor is currently allocated
  std::lock_guard<std::mutex> lock(mutex_);
  return allocatedCbvSrvUav_[index];
}

bool DescriptorHeapManager::isValidSamplerIndex(uint32_t index) const {
  if (index == UINT32_MAX) {
    return false;  // Sentinel value for allocation failure
  }
  if (index >= sizes_.samplers) {
    return false;  // Out of bounds
  }
  // Check if descriptor is currently allocated
  std::lock_guard<std::mutex> lock(mutex_);
  return allocatedSamplers_[index];
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

void DescriptorHeapManager::validateAndClampSizes(ID3D12Device* device) {
  // A-006: Validate descriptor heap sizes against D3D12 device limits
  IGL_LOG_INFO("=== Descriptor Heap Size Validation ===\n");

  // Query device options for resource binding tier (affects limits)
  D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
  HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                                            &options,
                                            sizeof(options));

  if (SUCCEEDED(hr)) {
    const char* tierName = "Unknown";
    switch (options.ResourceBindingTier) {
      case D3D12_RESOURCE_BINDING_TIER_1: tierName = "Tier 1"; break;
      case D3D12_RESOURCE_BINDING_TIER_2: tierName = "Tier 2"; break;
      case D3D12_RESOURCE_BINDING_TIER_3: tierName = "Tier 3"; break;
    }
    IGL_LOG_INFO("  Resource Binding Tier: %s\n", tierName);
  }

  // === SHADER-VISIBLE CBV/SRV/UAV HEAP ===
  // D3D12 spec: Max 1,000,000 descriptors for shader-visible heaps (FL 11.0+)
  // Conservative limit: 1,000,000 (actual limit may be lower on some hardware)
  constexpr uint32_t kMaxCbvSrvUavDescriptors = 1000000;

  if (sizes_.cbvSrvUav > kMaxCbvSrvUavDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested CBV/SRV/UAV heap size (%u) exceeds "
                  "D3D12 spec limit (%u)\n",
                  sizes_.cbvSrvUav, kMaxCbvSrvUavDescriptors);
    IGL_LOG_ERROR("  Clamping to %u descriptors\n", kMaxCbvSrvUavDescriptors);
    sizes_.cbvSrvUav = kMaxCbvSrvUavDescriptors;
  } else {
    IGL_LOG_INFO("  CBV/SRV/UAV heap size: %u (limit: %u) - OK\n",
                 sizes_.cbvSrvUav, kMaxCbvSrvUavDescriptors);
  }

  // === SHADER-VISIBLE SAMPLER HEAP ===
  // D3D12 spec: Max 2,048 descriptors (D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE)
  constexpr uint32_t kMaxSamplerDescriptors = 2048;

  if (sizes_.samplers > kMaxSamplerDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested sampler heap size (%u) exceeds "
                  "D3D12 limit (%u)\n",
                  sizes_.samplers, kMaxSamplerDescriptors);
    IGL_LOG_ERROR("  Clamping to %u descriptors\n", kMaxSamplerDescriptors);
    sizes_.samplers = kMaxSamplerDescriptors;
  } else {
    IGL_LOG_INFO("  Sampler heap size: %u (limit: %u) - OK\n",
                 sizes_.samplers, kMaxSamplerDescriptors);
  }

  // === CPU-VISIBLE RTV HEAP ===
  // D3D12 spec: Typically 64K+ descriptors (device-dependent)
  // Conservative validation: Warn if exceeding 16K (reasonable limit)
  constexpr uint32_t kMaxRtvDescriptors = 16384;

  if (sizes_.rtvs > kMaxRtvDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested RTV heap size (%u) is unusually large\n",
                  sizes_.rtvs);
    IGL_LOG_ERROR("  Recommended maximum: %u descriptors\n", kMaxRtvDescriptors);
    // Don't clamp - let CreateDescriptorHeap fail if truly excessive
  } else {
    IGL_LOG_INFO("  RTV heap size: %u (recommended max: %u) - OK\n",
                 sizes_.rtvs, kMaxRtvDescriptors);
  }

  // === CPU-VISIBLE DSV HEAP ===
  // Similar limits to RTV heap
  constexpr uint32_t kMaxDsvDescriptors = 16384;

  if (sizes_.dsvs > kMaxDsvDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested DSV heap size (%u) is unusually large\n",
                  sizes_.dsvs);
    IGL_LOG_ERROR("  Recommended maximum: %u descriptors\n", kMaxDsvDescriptors);
    // Don't clamp - let CreateDescriptorHeap fail if truly excessive
  } else {
    IGL_LOG_INFO("  DSV heap size: %u (recommended max: %u) - OK\n",
                 sizes_.dsvs, kMaxDsvDescriptors);
  }

  IGL_LOG_INFO("========================================\n");
}

} // namespace igl::d3d12
