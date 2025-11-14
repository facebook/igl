# I-006: Pipeline Cache Format Not Portable

**Severity:** Medium
**Category:** Cross-Platform Portability & Performance Optimization
**Status:** Open
**Related Issues:** G-008 (PSO Cache Miss Rate), None (blocking)

---

## Problem Statement

The D3D12 backend's **Pipeline State Object (PSO) cache** uses an **in-memory hash map** that is **not serializable or portable** across application runs. This means:

**The Impact:**
- **No persistent PSO cache**: Every application launch recreates all PSOs from scratch
- **Cold start performance penalty**: First frame/scene has massive PSO compilation cost
- **Inconsistent with Vulkan Pipeline Cache**: Vulkan provides `vkCreatePipelineCache` / `vkGetPipelineCacheData` for serialization
- **No cross-platform cache sharing**: Vulkan can serialize caches, D3D12 cannot
- **Wasted GPU driver optimizations**: Driver-compiled PSO blobs discarded on exit
- **Production impact**: 100ms+ stutter on first draw after application restart
- **Testing challenges**: Cannot pre-warm PSO cache for benchmarking

**Real-World Scenario:**
```cpp
// Session 1: Application first run
auto device = createDevice(BackendType::D3D12);

// Create 100 PSOs (expensive compilation)
for (int i = 0; i < 100; i++) {
  RenderPipelineDesc desc;
  // ... configure desc ...
  auto pso = device->createRenderPipeline(desc, nullptr);
  // Each PSO creation: 1-5ms (total: 100-500ms!)
}
// Total compilation time: 100-500ms

// Application exits - PSO cache discarded!

// Session 2: Application relaunch
auto device2 = createDevice(BackendType::D3D12);

// Same 100 PSOs must be recompiled AGAIN!
// Another 100-500ms wasted!
// NO benefit from previous session's compilation work!

// Vulkan comparison:
// - Session 1: Compile PSOs, serialize cache to disk
// - Session 2: Load cache from disk, PSOs instantly available (< 1ms each)
```

---

## Root Cause Analysis

### Current Implementation - Non-Persistent Cache:

#### Device.cpp (PSO Cache - Lines 1216-1296)
```cpp
// PSO Cache lookup (P0_DX12-001, H-013: Thread-safe with double-checked locking)
const size_t psoHash = hashComputePipelineDesc(desc);
Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

// First check: Lock for cache lookup
{
  std::lock_guard<std::mutex> lock(psoCacheMutex_);
  auto psoIt = computePSOCache_.find(psoHash);
  if (psoIt != computePSOCache_.end()) {
    // Cache hit - reuse existing PSO
    computePSOCacheHits_++;
    pipelineState = psoIt->second;  // In-memory only!
    // ... return cached PSO ...
  }
}

// Cache miss - create new PSO outside lock (expensive operation)
// ... CreateComputePipelineState() call (1-5ms) ...

// Second check: Lock for cache insertion
{
  std::lock_guard<std::mutex> lock(psoCacheMutex_);
  // ... insert into in-memory cache ...
  computePSOCache_[psoHash] = pipelineState;  // NOT persisted to disk!
}
```

**Problem:** `computePSOCache_` and `renderPSOCache_` are `std::unordered_map` in RAM only!

#### Device.h (PSO Cache Members)
```cpp
class Device final : public IDevice {
 private:
  // In-memory PSO caches (NOT serializable)
  mutable std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>> computePSOCache_;
  mutable std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>> renderPSOCache_;

  // Cache statistics (NOT persisted)
  mutable size_t computePSOCacheHits_ = 0;
  mutable size_t computePSOCacheMisses_ = 0;
  mutable size_t renderPSOCacheHits_ = 0;
  mutable size_t renderPSOCacheMisses_ = 0;

  // NO serialization/deserialization methods!
};
```

**Problem:** No API to serialize/deserialize PSO cache to/from disk!

### D3D12 PSO Serialization API (Exists but Not Used):

D3D12 provides **`CachedPSO`** field in pipeline state descriptors:

```cpp
// D3D12_COMPUTE_PIPELINE_STATE_DESC
struct D3D12_COMPUTE_PIPELINE_STATE_DESC {
  // ... other fields ...
  D3D12_CACHED_PIPELINE_STATE CachedPSO;  // ← NOT USED IN IGL!
  // ...
};

// D3D12_CACHED_PIPELINE_STATE
struct D3D12_CACHED_PIPELINE_STATE {
  const void* pCachedBlob;          // Pointer to serialized PSO blob
  SIZE_T CachedBlobSizeInBytes;     // Size of serialized blob
};
```

**Current IGL Code:**
```cpp
// Device.cpp line 1212-1213
psoDesc.CachedPSO.pCachedBlob = nullptr;  // ← Always null!
psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;  // ← Always zero!
```

**Available D3D12 API (Not Used):**
```cpp
// Serialize PSO blob for caching
ID3D12PipelineState::GetCachedBlob(ID3DBlob** ppBlob);
```

### Vulkan Comparison (Persistent Cache Supported):

Vulkan provides full pipeline cache serialization:

```cpp
// Create pipeline cache (optionally from serialized data)
VkPipelineCacheCreateInfo cacheInfo = {};
cacheInfo.initialDataSize = loadedCacheSize;  // From disk
cacheInfo.pInitialData = loadedCacheData;     // From disk
vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache);

// Use cache when creating pipelines
vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);

// Serialize cache to disk
size_t cacheSize;
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, nullptr);
std::vector<uint8_t> cacheData(cacheSize);
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, cacheData.data());
// Write cacheData to disk
```

### Why This Is Wrong:

1. **Performance Regression**: Cold start penalty every launch (100ms-500ms)
2. **Inconsistent with Vulkan**: Vulkan supports persistence, D3D12 does not
3. **Unused D3D12 Feature**: `CachedPSO` field available but never used
4. **Driver Optimization Loss**: GPU driver compiles optimized PSO blobs, then discards them
5. **No Pre-Warming**: Cannot load pre-compiled PSO cache for testing/benchmarking
6. **Scalability Issue**: Large applications with 1000s of PSOs suffer most

---

## Official Documentation References

### D3D12 Documentation:

1. **D3D12_CACHED_PIPELINE_STATE**:
   - [D3D12_CACHED_PIPELINE_STATE Structure](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_cached_pipeline_state)
   - Quote: "Stores a pipeline state in a driver-specific format. Applications can serialize and deserialize pipeline state objects to disk."
   - Fields: `pCachedBlob`, `CachedBlobSizeInBytes`

2. **ID3D12PipelineState::GetCachedBlob**:
   - [GetCachedBlob Method](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinestate-getcachedblob)
   - Quote: "Gets the cached blob representing the pipeline state. This can be used to serialize the PSO to disk."
   - Returns: `ID3DBlob` containing driver-specific cached PSO data

3. **Best Practices - PSO Caching**:
   - [Pipeline State Objects - Caching](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12#pipeline-state-object-caching)
   - Quote: "Applications should cache pipeline state objects to avoid recreation costs. Use GetCachedBlob() to serialize PSOs for future use."

4. **Pipeline State Streams** (Alternative Approach):
   - [ID3D12PipelineState - Remarks](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12pipelinestate)
   - Quote: "Drivers can optimize PSO creation when provided with cached blobs from previous creations."

### Vulkan Documentation:

1. **Pipeline Caching**:
   - [vkCreatePipelineCache](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCreatePipelineCache.html)
   - [vkGetPipelineCacheData](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPipelineCacheData.html)
   - Quote: "Pipeline cache objects allow the result of pipeline construction to be reused between pipelines and between runs of an application."

2. **Cache Serialization**:
   - [VkPipelineCacheCreateInfo](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineCacheCreateInfo.html)
   - Quote: "Applications can serialize pipeline cache data to external memory and use it to initialize future pipeline cache objects."

---

## Code Location Strategy

### Files to Modify:

1. **Device.h** (Add serialization API):
   - Location: `src/igl/d3d12/Device.h` (after line 100)
   - Context: Device class public methods
   - Action: Add PSO cache serialization/deserialization API

2. **Device.cpp** (Implement serialization):
   - Location: `src/igl/d3d12/Device.cpp:1212-1213` (CachedPSO initialization)
   - Search for: `psoDesc.CachedPSO.pCachedBlob = nullptr;`
   - Context: PSO creation with cached blob
   - Action: Check for cached blob before PSO creation, call `GetCachedBlob()` after creation

3. **Device.cpp** (Add cache management methods):
   - Location: End of `Device.cpp` (after existing methods)
   - Context: New methods for cache serialization
   - Action: Implement `serializePSOCache()`, `deserializePSOCache()`, `clearPSOCache()`

### Files to Review (No Changes Expected):

1. **RenderPipelineState.cpp** / **ComputePipelineState.cpp**
   - Pipeline state wrapper classes - no changes needed

---

## Detection Strategy

### How to Reproduce the Missing Persistence:

#### Test 1: Measure Cold Start PSO Compilation Time
```cpp
// Session 1: Create PSOs and measure time
auto device = createDevice(BackendType::D3D12);

auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 100; i++) {
  RenderPipelineDesc desc;
  // ... configure desc ...
  auto pso = device->createRenderPipeline(desc, nullptr);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

std::cout << "Session 1 PSO compilation: " << duration << "ms\n";
// CURRENT: 100-500ms (no cache)

// Session 2: Restart application, measure time again
// EXPECTED (with cache): < 10ms (loaded from disk)
// CURRENT (no cache): 100-500ms (same as session 1)
```

#### Test 2: Verify PSO Cache Blob Serialization
```cpp
auto device = createDevice(BackendType::D3D12);

// Create PSO
RenderPipelineDesc desc;
// ... configure desc ...
Result result;
auto pso = device->createRenderPipeline(desc, &result);

// Try to get cached blob (D3D12 API)
auto d3d12Device = static_cast<igl::d3d12::Device*>(device.get());
auto d3d12PSO = /* get underlying ID3D12PipelineState */;

Microsoft::WRL::ComPtr<ID3DBlob> cachedBlob;
HRESULT hr = d3d12PSO->GetCachedBlob(cachedBlob.GetAddressOf());

if (SUCCEEDED(hr) && cachedBlob) {
  std::cout << "Cached blob size: " << cachedBlob->GetBufferSize() << " bytes\n";
  // Blob can be serialized to disk!
} else {
  std::cout << "GetCachedBlob FAILED or returned null\n";
}

// CURRENT: No IGL API to access cached blobs for serialization
```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add PSO Cache Serialization API to Device

**File:** `src/igl/d3d12/Device.h`

**Add public methods (after line 100):**
```cpp
class Device final : public IDevice {
 public:
  // ... existing methods ...

  // PSO Cache Serialization (for persistent caching across application runs)
  //
  // Serialize current PSO cache to binary blob for saving to disk
  // Returns serialized cache data (can be written to file)
  [[nodiscard]] std::vector<uint8_t> serializePSOCache() const;

  // Deserialize PSO cache from binary blob (loaded from disk)
  // Should be called early in initialization before creating pipelines
  // Returns Result::Ok if successful, error otherwise
  [[nodiscard]] Result deserializePSOCache(const std::vector<uint8_t>& cacheData);

  // Clear PSO cache (useful for testing or low-memory scenarios)
  void clearPSOCache();

  // Get PSO cache statistics (hits/misses for telemetry)
  struct PSOCacheStats {
    size_t renderCacheHits = 0;
    size_t renderCacheMisses = 0;
    size_t computeCacheHits = 0;
    size_t computeCacheMisses = 0;
    size_t totalEntries = 0;
    size_t estimatedSizeBytes = 0;
  };
  [[nodiscard]] PSOCacheStats getPSOCacheStats() const;

  // ... rest of class ...
};
```

---

#### Step 2: Implement Cache Serialization Format

**File:** `src/igl/d3d12/Device.cpp` (add at end of file)

**Define serialization format:**
```cpp
namespace {
// PSO Cache Serialization Format (Version 1)
// Header: [Magic: 4 bytes] [Version: 4 bytes] [NumEntries: 4 bytes]
// Entry:  [HashKey: 8 bytes] [BlobSize: 4 bytes] [BlobData: BlobSize bytes]
constexpr uint32_t kPSOCacheMagic = 0x50534F43;  // "PSOC"
constexpr uint32_t kPSOCacheVersion = 1;

struct PSOCacheHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t numEntries;
};

struct PSOCacheEntry {
  size_t hashKey;
  uint32_t blobSize;
  // Followed by blobSize bytes of blob data
};
} // namespace

std::vector<uint8_t> Device::serializePSOCache() const {
  std::vector<uint8_t> data;

  // Count total entries
  std::lock_guard<std::mutex> lock(psoCacheMutex_);
  const uint32_t numEntries = static_cast<uint32_t>(renderPSOCache_.size() + computePSOCache_.size());

  if (numEntries == 0) {
    IGL_LOG_INFO("Device::serializePSOCache: No PSOs to serialize\n");
    return data;  // Empty cache
  }

  // Estimate size (header + entries)
  size_t estimatedSize = sizeof(PSOCacheHeader);
  // Each entry: hashKey (8) + blobSize (4) + blob data (estimate 4KB average)
  estimatedSize += numEntries * (sizeof(size_t) + sizeof(uint32_t) + 4096);
  data.reserve(estimatedSize);

  // Write header
  PSOCacheHeader header;
  header.magic = kPSOCacheMagic;
  header.version = kPSOCacheVersion;
  header.numEntries = numEntries;
  data.insert(data.end(), reinterpret_cast<const uint8_t*>(&header),
              reinterpret_cast<const uint8_t*>(&header) + sizeof(header));

  // Serialize render PSOs
  for (const auto& [hashKey, pso] : renderPSOCache_) {
    Microsoft::WRL::ComPtr<ID3DBlob> cachedBlob;
    HRESULT hr = pso->GetCachedBlob(cachedBlob.GetAddressOf());

    if (FAILED(hr) || !cachedBlob) {
      IGL_LOG_WARNING("Device::serializePSOCache: Failed to get cached blob for render PSO hash 0x%zx\n", hashKey);
      continue;
    }

    // Write entry: [hashKey][blobSize][blobData]
    const uint32_t blobSize = static_cast<uint32_t>(cachedBlob->GetBufferSize());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&hashKey),
                reinterpret_cast<const uint8_t*>(&hashKey) + sizeof(hashKey));
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&blobSize),
                reinterpret_cast<const uint8_t*>(&blobSize) + sizeof(blobSize));
    data.insert(data.end(), static_cast<const uint8_t*>(cachedBlob->GetBufferPointer()),
                static_cast<const uint8_t*>(cachedBlob->GetBufferPointer()) + blobSize);
  }

  // Serialize compute PSOs (same pattern)
  for (const auto& [hashKey, pso] : computePSOCache_) {
    Microsoft::WRL::ComPtr<ID3DBlob> cachedBlob;
    HRESULT hr = pso->GetCachedBlob(cachedBlob.GetAddressOf());

    if (FAILED(hr) || !cachedBlob) {
      IGL_LOG_WARNING("Device::serializePSOCache: Failed to get cached blob for compute PSO hash 0x%zx\n", hashKey);
      continue;
    }

    const uint32_t blobSize = static_cast<uint32_t>(cachedBlob->GetBufferSize());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&hashKey),
                reinterpret_cast<const uint8_t*>(&hashKey) + sizeof(hashKey));
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&blobSize),
                reinterpret_cast<const uint8_t*>(&blobSize) + sizeof(blobSize));
    data.insert(data.end(), static_cast<const uint8_t*>(cachedBlob->GetBufferPointer()),
                static_cast<const uint8_t*>(cachedBlob->GetBufferPointer()) + blobSize);
  }

  IGL_LOG_INFO("Device::serializePSOCache: Serialized %u PSOs (%zu bytes)\n",
               numEntries, data.size());

  return data;
}
```

---

#### Step 3: Implement Cache Deserialization

**File:** `src/igl/d3d12/Device.cpp` (continue in same file)

```cpp
Result Device::deserializePSOCache(const std::vector<uint8_t>& cacheData) {
  if (cacheData.empty()) {
    return Result(Result::Code::ArgumentInvalid, "Empty cache data");
  }

  if (cacheData.size() < sizeof(PSOCacheHeader)) {
    return Result(Result::Code::ArgumentInvalid, "Cache data too small for header");
  }

  // Read header
  PSOCacheHeader header;
  std::memcpy(&header, cacheData.data(), sizeof(header));

  if (header.magic != kPSOCacheMagic) {
    return Result(Result::Code::ArgumentInvalid, "Invalid PSO cache magic");
  }

  if (header.version != kPSOCacheVersion) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Unsupported PSO cache version: %u (expected %u)",
             header.version, kPSOCacheVersion);
    return Result(Result::Code::Unsupported, msg);
  }

  IGL_LOG_INFO("Device::deserializePSOCache: Loading %u cached PSOs\n", header.numEntries);

  // NOTE: We store the serialized blobs in a separate map
  // When createRenderPipeline() / createComputePipeline() is called,
  // we check this map and provide the cached blob to D3D12

  std::lock_guard<std::mutex> lock(psoCacheMutex_);

  // Parse entries
  size_t offset = sizeof(PSOCacheHeader);
  for (uint32_t i = 0; i < header.numEntries; i++) {
    if (offset + sizeof(size_t) + sizeof(uint32_t) > cacheData.size()) {
      IGL_LOG_ERROR("Device::deserializePSOCache: Truncated cache data at entry %u\n", i);
      break;
    }

    // Read hashKey
    size_t hashKey;
    std::memcpy(&hashKey, cacheData.data() + offset, sizeof(hashKey));
    offset += sizeof(hashKey);

    // Read blobSize
    uint32_t blobSize;
    std::memcpy(&blobSize, cacheData.data() + offset, sizeof(blobSize));
    offset += sizeof(blobSize);

    if (offset + blobSize > cacheData.size()) {
      IGL_LOG_ERROR("Device::deserializePSOCache: Truncated blob data at entry %u\n", i);
      break;
    }

    // Store blob data for later use (when PSO is created)
    std::vector<uint8_t> blobData(blobSize);
    std::memcpy(blobData.data(), cacheData.data() + offset, blobSize);
    offset += blobSize;

    // Store in a separate map: hash -> cached blob
    // (Defined as: mutable std::unordered_map<size_t, std::vector<uint8_t>> serializedPSOCache_;)
    serializedPSOCache_[hashKey] = std::move(blobData);
  }

  IGL_LOG_INFO("Device::deserializePSOCache: Loaded %zu cached PSO blobs\n",
               serializedPSOCache_.size());

  return Result();
}
```

---

#### Step 4: Use Cached Blobs During PSO Creation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Line 1207-1214 (compute PSO creation)

**Before:**
```cpp
D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = rootSignature.Get();
psoDesc.CS.pShaderBytecode = csBytecode.data();
psoDesc.CS.BytecodeLength = csBytecode.size();
psoDesc.NodeMask = 0;
psoDesc.CachedPSO.pCachedBlob = nullptr;  // ← NOT USED
psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;  // ← NOT USED
psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
```

**After:**
```cpp
D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = rootSignature.Get();
psoDesc.CS.pShaderBytecode = csBytecode.data();
psoDesc.CS.BytecodeLength = csBytecode.size();
psoDesc.NodeMask = 0;

// Check if we have a cached blob for this PSO hash
const size_t psoHash = hashComputePipelineDesc(desc);
{
  std::lock_guard<std::mutex> lock(psoCacheMutex_);
  auto it = serializedPSOCache_.find(psoHash);
  if (it != serializedPSOCache_.end()) {
    // Use cached blob to accelerate PSO creation
    psoDesc.CachedPSO.pCachedBlob = it->second.data();
    psoDesc.CachedPSO.CachedBlobSizeInBytes = it->second.size();
    IGL_LOG_INFO("Device::createComputePipeline: Using cached PSO blob (hash=0x%zx, size=%zu)\n",
                 psoHash, it->second.size());
  } else {
    psoDesc.CachedPSO.pCachedBlob = nullptr;
    psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  }
}

psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
```

**Apply similar changes to render PSO creation** (around line 1500+)

---

#### Step 5: Add Cache Management Methods

**File:** `src/igl/d3d12/Device.cpp`

```cpp
void Device::clearPSOCache() {
  std::lock_guard<std::mutex> lock(psoCacheMutex_);
  renderPSOCache_.clear();
  computePSOCache_.clear();
  serializedPSOCache_.clear();
  renderPSOCacheHits_ = 0;
  renderPSOCacheMisses_ = 0;
  computePSOCacheHits_ = 0;
  computePSOCacheMisses_ = 0;
  IGL_LOG_INFO("Device::clearPSOCache: All PSO caches cleared\n");
}

Device::PSOCacheStats Device::getPSOCacheStats() const {
  std::lock_guard<std::mutex> lock(psoCacheMutex_);

  PSOCacheStats stats;
  stats.renderCacheHits = renderPSOCacheHits_;
  stats.renderCacheMisses = renderPSOCacheMisses_;
  stats.computeCacheHits = computePSOCacheHits_;
  stats.computeCacheMisses = computePSOCacheMisses_;
  stats.totalEntries = renderPSOCache_.size() + computePSOCache_.size();

  // Estimate cache size (PSO objects + serialized blobs)
  stats.estimatedSizeBytes = stats.totalEntries * 4096;  // Rough estimate

  return stats;
}
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 PSO cache tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*PSO*Cache*"

# Run pipeline creation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Pipeline*"

# Run all render sessions (should not regress)
./test_all_sessions.bat
```

**Expected Results:**
- All existing tests pass
- PSO cache serialization/deserialization works
- Cached PSOs load significantly faster (10-50x speedup)

### New Test Cases Required:

#### Test 1: PSO Cache Serialization
```cpp
TEST_F(DeviceD3D12Test, PSOCacheSerialization) {
  // Create PSOs
  for (int i = 0; i < 10; i++) {
    RenderPipelineDesc desc;
    // ... configure desc ...
    device_->createRenderPipeline(desc, nullptr);
  }

  // Serialize cache
  auto cacheData = device_->serializePSOCache();
  EXPECT_GT(cacheData.size(), 0u);

  // Clear cache
  device_->clearPSOCache();

  // Deserialize cache
  Result result = device_->deserializePSOCache(cacheData);
  EXPECT_TRUE(result.isOk()) << result.message;
}
```

#### Test 2: Cached PSO Performance
```cpp
TEST_F(DeviceD3D12Test, CachedPSOPerformance) {
  RenderPipelineDesc desc;
  // ... configure desc ...

  // First creation (cold)
  auto start1 = std::chrono::high_resolution_clock::now();
  auto pso1 = device_->createRenderPipeline(desc, nullptr);
  auto end1 = std::chrono::high_resolution_clock::now();
  auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();

  // Serialize and deserialize cache
  auto cacheData = device_->serializePSOCache();
  device_->clearPSOCache();
  device_->deserializePSOCache(cacheData);

  // Second creation (warm, with cached blob)
  auto start2 = std::chrono::high_resolution_clock::now();
  auto pso2 = device_->createRenderPipeline(desc, nullptr);
  auto end2 = std::chrono::high_resolution_clock::now();
  auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();

  std::cout << "Cold PSO creation: " << duration1 << "us\n";
  std::cout << "Warm PSO creation: " << duration2 << "us\n";

  // Expect significant speedup (at least 2x)
  EXPECT_LT(duration2, duration1 / 2);
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass with no regression
./test_all_sessions.bat
```

**Expected Changes:**
- No behavior change for existing sessions
- Optional: Add cache serialization to application shutdown

**Test Modifications Allowed:**
- Add D3D12 backend-specific PSO cache tests
- DO NOT modify cross-platform test logic

---

## Definition of Done

### Completion Criteria:

- [ ] PSO cache serialization API added to Device
- [ ] PSO cache deserialization API added to Device
- [ ] Cached blobs used during PSO creation (D3D12 CachedPSO field populated)
- [ ] All unit tests pass (including new serialization tests)
- [ ] All render sessions pass
- [ ] Cached PSO creation 10-50x faster than cold creation
- [ ] Cache format documented with version handling
- [ ] Cache management methods implemented (clear, stats)
- [ ] Documentation comments added

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**

1. All D3D12 PSO cache tests pass
2. Serialization/deserialization tests pass
3. Cached PSO performance improvement validated (10-50x speedup)
4. All render sessions continue working

**Post in chat:**
```
I-006 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- PSO cache serialization: IMPLEMENTED
- PSO cache deserialization: IMPLEMENTED
- Cold PSO creation: [X]ms (report actual)
- Warm PSO creation: [Y]ms (report actual, should be 10-50x faster)
- Speedup: [X/Y]x

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix)

### Blocks:
- Cold start performance optimization
- Pre-warmed PSO cache for benchmarking
- Cross-platform cache sharing (Vulkan + D3D12)

### Related:
- **G-008** (PSO Cache Miss Rate) - This fix enables persistent caching to improve hit rate

---

## Implementation Priority

**Priority:** P2 - MEDIUM (Performance Optimization)
**Estimated Effort:** 4-6 hours
**Risk:** MEDIUM (serialization format requires versioning, cache invalidation handling)
**Impact:** HIGH - Eliminates cold start PSO compilation penalty (100-500ms savings)

**Rationale:**
- Significant cold start performance improvement
- Aligns D3D12 with Vulkan's persistent cache capabilities
- Uses existing D3D12 API (CachedPSO, GetCachedBlob)
- Requires careful serialization format design and validation

---

## References

### D3D12:
- [D3D12_CACHED_PIPELINE_STATE](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_cached_pipeline_state)
- [ID3D12PipelineState::GetCachedBlob](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinestate-getcachedblob)
- [Pipeline State Objects - Caching](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12#pipeline-state-object-caching)

### Vulkan:
- [vkCreatePipelineCache](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCreatePipelineCache.html)
- [vkGetPipelineCacheData](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetPipelineCacheData.html)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
