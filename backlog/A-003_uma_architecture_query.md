# A-003: Missing UMA Architecture Query

**Severity:** Medium
**Category:** Architecture Detection
**Status:** Open
**Related Issues:** P1_DX12-008 (Device Limits), A-006 (Heap Size Validation)

---

## Problem Statement

The D3D12 backend does not query `D3D12_FEATURE_ARCHITECTURE` to detect Unified Memory Architecture (UMA) systems. This means the implementation cannot optimize memory management strategies for integrated GPUs (Intel HD/Iris, AMD APUs, ARM SoCs) versus discrete GPUs (NVIDIA GeForce, AMD Radeon).

**Impact Analysis:**
- **Memory Efficiency:** Cannot optimize upload strategies for UMA systems where CPU and GPU share the same physical memory
- **Performance:** Missing opportunity to use `D3D12_HEAP_TYPE_CUSTOM` with `D3D12_MEMORY_POOL_L0` on UMA systems for zero-copy uploads
- **Mobile/Laptop:** Integrated GPUs are increasingly common (Intel 12th+ gen, Apple M-series via translation layers, ARM devices)

**The Danger:**
- Over-allocating upload heaps on UMA systems where direct GPU access to system memory is efficient
- Under-utilizing integrated GPU capabilities by forcing unnecessary copies
- Incorrect memory pool selection for custom heaps, leading to sub-optimal performance

---

## Root Cause Analysis

### Current Implementation (`Device.cpp:61-110`):

```cpp
void Device::validateDeviceLimits() {
  auto* device = ctx_->getDevice();
  if (!device) {
    IGL_LOG_ERROR("Device::validateDeviceLimits: D3D12 device is null\n");
    return;
  }

  IGL_LOG_INFO("=== D3D12 Device Capabilities and Limits Validation ===\n");

  // Query D3D12_FEATURE_D3D12_OPTIONS for resource binding tier
  HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &deviceOptions_, sizeof(deviceOptions_));

  // ... logs resource binding tier ...

  // Query D3D12_FEATURE_D3D12_OPTIONS1 for root signature version
  hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &deviceOptions1_, sizeof(deviceOptions1_));

  // ... but NO query for D3D12_FEATURE_ARCHITECTURE ...
}
```

### Why This Is Wrong:

1. **No UMA Detection:** The code queries `D3D12_OPTIONS` and `D3D12_OPTIONS1` but never checks `D3D12_FEATURE_ARCHITECTURE`, which provides critical information about memory architecture
2. **Blind Upload Strategy:** Upload ring buffer (`UploadRingBuffer.cpp`) uses fixed `D3D12_HEAP_TYPE_UPLOAD` without considering UMA optimizations
3. **Missing Cache Coherency Info:** Cannot determine if cache coherent UMA is available (`CacheCoherentUMA` flag in architecture query)

---

## Official Documentation References

1. **D3D12_FEATURE_ARCHITECTURE Enumeration**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
   - Key guidance: "Describes the architecture of a D3D12 adapter, including whether the adapter has UMA and if cache coherent UMA is supported"

2. **D3D12_FEATURE_DATA_ARCHITECTURE Structure**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_architecture
   - Contains `UMA` (Unified Memory Architecture) and `CacheCoherentUMA` flags
   - NodeIndex: 0 for single GPU, varies for multi-GPU

3. **Memory Architecture Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/memory-management
   - Guidance on custom heaps: "On UMA architectures, applications can use D3D12_MEMORY_POOL_L0 for upload heaps to enable GPU direct access to system memory"

4. **GPU Architecture and Memory**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MemoryManagement
   - Sample demonstrates architecture query for optimal heap selection

---

## Code Location Strategy

### Files to Modify:

1. **Device.h** (`Device` class):
   - Search for: `D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_;`
   - Context: Private member variables storing feature query results
   - Action: Add `D3D12_FEATURE_DATA_ARCHITECTURE architectureInfo_;` member

2. **Device.cpp** (`validateDeviceLimits` method):
   - Search for: `device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1`
   - Context: Feature queries during device initialization
   - Action: Add architecture query after OPTIONS1 query

3. **Device.h** (`Device` class public methods):
   - Search for: `const D3D12_FEATURE_DATA_D3D12_OPTIONS& getDeviceOptions()`
   - Context: Getter methods for feature support
   - Action: Add `bool isUMA() const` and `bool isCacheCoherentUMA() const` accessors

4. **UploadRingBuffer.cpp** (constructor):
   - Search for: `D3D12_HEAP_PROPERTIES heapProps = {};`
   - Context: Upload heap creation with hardcoded UPLOAD heap type
   - Action: Conditionally use CUSTOM heap with L0 pool on UMA systems

---

## Detection Strategy

### How to Reproduce:

Run on integrated GPU system (Intel HD/Iris, AMD APU):
```cpp
// Check logs during device initialization
// Expected: NO log about UMA architecture
// Actual: validateDeviceLimits() only logs OPTIONS and OPTIONS1
```

### Verification After Fix:

1. Build and run on integrated GPU (UMA) system
2. Check logs for UMA architecture detection:
   ```
   === D3D12 Device Capabilities and Limits Validation ===
   ...
   Architecture: UMA=Yes, CacheCoherentUMA=Yes
   ```
3. Run on discrete GPU (NVIDIA/AMD) system
4. Check logs confirm non-UMA architecture:
   ```
   Architecture: UMA=No, CacheCoherentUMA=No
   ```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Architecture Query Member Variables

**File:** `src/igl/d3d12/Device.h`

**Locate:** Search for `D3D12_FEATURE_DATA_D3D12_OPTIONS1 deviceOptions1_;`

**Current (PROBLEM):**
```cpp
class Device {
 private:
  D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 deviceOptions1_ = {};
  // No architecture info!
};
```

**Fixed (SOLUTION):**
```cpp
class Device {
 private:
  D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 deviceOptions1_ = {};
  // A-003: Store UMA architecture information
  D3D12_FEATURE_DATA_ARCHITECTURE architectureInfo_ = {};
};
```

**Rationale:** Store architecture query results for runtime access by upload strategies.

---

#### Step 2: Add Architecture Query in validateDeviceLimits()

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Search for the end of `CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1` block

**Current (PROBLEM):**
```cpp
hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &deviceOptions1_, sizeof(deviceOptions1_));

if (SUCCEEDED(hr)) {
  IGL_LOG_INFO("  Wave Intrinsics Supported: %s\n",
               deviceOptions1_.WaveOps ? "Yes" : "No");
  // ... more OPTIONS1 logs ...
} else {
  IGL_LOG_INFO("  D3D12_FEATURE_D3D12_OPTIONS1 query failed (not critical)\n");
}

// Immediately goes to limit validation - NO architecture query!
IGL_LOG_INFO("\n=== Limit Validation ===\n");
```

**Fixed (SOLUTION):**
```cpp
hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &deviceOptions1_, sizeof(deviceOptions1_));

if (SUCCEEDED(hr)) {
  IGL_LOG_INFO("  Wave Intrinsics Supported: %s\n",
               deviceOptions1_.WaveOps ? "Yes" : "No");
  // ... more OPTIONS1 logs ...
} else {
  IGL_LOG_INFO("  D3D12_FEATURE_D3D12_OPTIONS1 query failed (not critical)\n");
}

// A-003: Query UMA architecture information
architectureInfo_.NodeIndex = 0; // Query primary GPU (node 0)
hr = device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE,
                                  &architectureInfo_,
                                  sizeof(architectureInfo_));

if (SUCCEEDED(hr)) {
  IGL_LOG_INFO("  Architecture Information:\n");
  IGL_LOG_INFO("    - UMA (Unified Memory Architecture): %s\n",
               architectureInfo_.UMA ? "Yes (integrated GPU)" : "No (discrete GPU)");
  IGL_LOG_INFO("    - Cache Coherent UMA: %s\n",
               architectureInfo_.CacheCoherentUMA ? "Yes" : "No");

  // Log implications for memory management
  if (architectureInfo_.UMA) {
    IGL_LOG_INFO("    => Optimization: Can use D3D12_MEMORY_POOL_L0 for upload heaps\n");
    IGL_LOG_INFO("    => Optimization: Reduced upload overhead due to shared memory\n");
  } else {
    IGL_LOG_INFO("    => Standard upload heap strategy for discrete GPU\n");
  }
} else {
  // Architecture query failure is non-critical, assume discrete GPU
  IGL_LOG_INFO("  Architecture query failed (HRESULT: 0x%08X), assuming discrete GPU\n", hr);
  architectureInfo_.UMA = FALSE;
  architectureInfo_.CacheCoherentUMA = FALSE;
}

// Continue with limit validation
IGL_LOG_INFO("\n=== Limit Validation ===\n");
```

**Rationale:** Query architecture during initialization to inform upload strategies. Log results for diagnostic visibility. Gracefully handle query failure by assuming discrete GPU (safest default).

---

#### Step 3: Add Public Accessors for Architecture Info

**File:** `src/igl/d3d12/Device.h`

**Locate:** Search for public getter methods like `getDeviceOptions()`

**Current (PROBLEM):**
```cpp
class Device {
 public:
  const D3D12_FEATURE_DATA_D3D12_OPTIONS& getDeviceOptions() const { return deviceOptions_; }
  const D3D12_FEATURE_DATA_D3D12_OPTIONS1& getDeviceOptions1() const { return deviceOptions1_; }
  // No architecture accessors!
};
```

**Fixed (SOLUTION):**
```cpp
class Device {
 public:
  const D3D12_FEATURE_DATA_D3D12_OPTIONS& getDeviceOptions() const { return deviceOptions_; }
  const D3D12_FEATURE_DATA_D3D12_OPTIONS1& getDeviceOptions1() const { return deviceOptions1_; }

  // A-003: Accessors for UMA architecture information
  // Returns true if device uses Unified Memory Architecture (integrated GPU)
  bool isUMA() const { return architectureInfo_.UMA != 0; }

  // Returns true if UMA is cache coherent (CPU writes visible to GPU without flush)
  bool isCacheCoherentUMA() const { return architectureInfo_.CacheCoherentUMA != 0; }

  // Full architecture info for advanced users
  const D3D12_FEATURE_DATA_ARCHITECTURE& getArchitectureInfo() const {
    return architectureInfo_;
  }
};
```

**Rationale:** Provide simple boolean accessors for common use cases (isUMA check) and full structure access for advanced scenarios.

---

#### Step 4: (OPTIONAL) Update UploadRingBuffer to Use UMA Info

**File:** `src/igl/d3d12/UploadRingBuffer.cpp`

**Locate:** Search for `D3D12_HEAP_PROPERTIES heapProps = {};` in constructor

**Current (PROBLEM):**
```cpp
UploadRingBuffer::UploadRingBuffer(ID3D12Device* device, uint64_t size)
    : device_(device), size_(size) {
  // Create upload heap - hardcoded UPLOAD type
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Always UPLOAD, no UMA check
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  // ... rest of upload heap creation ...
}
```

**Fixed (SOLUTION):**
```cpp
UploadRingBuffer::UploadRingBuffer(ID3D12Device* device,
                                   uint64_t size,
                                   bool isUMA) // Add UMA flag parameter
    : device_(device), size_(size) {
  // A-003: Use optimal heap configuration based on UMA architecture
  D3D12_HEAP_PROPERTIES heapProps = {};

  if (isUMA) {
    // UMA optimization: Use CUSTOM heap with L0 pool for direct GPU access
    // to system memory, avoiding unnecessary copies
    heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // System memory
    IGL_LOG_INFO("UploadRingBuffer: Using UMA-optimized CUSTOM heap (L0 pool)\n");
  } else {
    // Discrete GPU: Use standard UPLOAD heap
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    IGL_LOG_INFO("UploadRingBuffer: Using standard UPLOAD heap (discrete GPU)\n");
  }

  // ... rest of upload heap creation ...
}
```

**Update Device.cpp constructor to pass UMA flag:**
```cpp
Device::Device(std::unique_ptr<D3D12Context> ctx) : ctx_(std::move(ctx)) {
  // ... existing code ...

  // Initialize upload ring buffer with UMA optimization (A-003)
  constexpr uint64_t kUploadRingBufferSize = 128 * 1024 * 1024;
  uploadRingBuffer_ = std::make_unique<UploadRingBuffer>(
      device,
      kUploadRingBufferSize,
      isUMA() // Pass UMA flag for optimal heap selection
  );
}
```

**Rationale:** On UMA systems, CUSTOM heap with L0 pool enables GPU to directly access system memory without intermediate copy, improving performance and reducing memory usage.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Device capability tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Device*"
```

**Test Modifications Allowed:**
- ✅ Add test to verify architecture query returns valid data
- ✅ Add test to verify isUMA() accessor works correctly
- ❌ **DO NOT modify cross-platform test logic**

**New Test to Add:**
```cpp
// Test architecture query populates data
TEST(D3D12Device, ArchitectureQueryReturnsValidData) {
  auto device = createTestDevice();
  auto& archInfo = device->getArchitectureInfo();

  // NodeIndex should be 0 for primary GPU
  EXPECT_EQ(archInfo.NodeIndex, 0);

  // UMA and CacheCoherentUMA are boolean flags
  // Either can be true or false, but if CacheCoherentUMA is true, UMA must be true
  if (archInfo.CacheCoherentUMA) {
    EXPECT_TRUE(archInfo.UMA);
  }
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass (no behavior change, only query addition)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
```

**Modifications Allowed:**
- ✅ None required - this is a query-only change
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (existing + new architecture query test)
- [ ] `architectureInfo_` member added to Device class
- [ ] Architecture query executed in `validateDeviceLimits()`
- [ ] Public accessors `isUMA()` and `isCacheCoherentUMA()` implemented
- [ ] Logs show architecture info on device initialization
- [ ] Tested on both UMA (integrated GPU) and non-UMA (discrete GPU) systems
- [ ] (Optional) UploadRingBuffer updated to use UMA optimization
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Architecture query logs appear correctly during device initialization
2. Test on integrated GPU shows `UMA=Yes`
3. Test on discrete GPU shows `UMA=No`

**Post in chat:**
```
A-003 Fix Complete - Ready for Review

Architecture Query Implementation:
- Query added: D3D12_FEATURE_ARCHITECTURE
- Accessors: isUMA(), isCacheCoherentUMA(), getArchitectureInfo()
- Logging: Shows UMA status and implications

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Integrated GPU test: UMA=Yes detected correctly
- Discrete GPU test: UMA=No detected correctly
- Render sessions: PASS (no behavior change)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- **G-006** - Upload ring buffer underutilization (needs UMA info for optimal strategy)

### Related:
- **P1_DX12-008** - Missing limit queries (similar feature query enhancement)
- **A-006** - Descriptor heap size validation (needs architecture info for heap limits)
- **G-003** - Descriptor heap fragmentation (UMA systems may have different heap size limits)

---

## Implementation Priority

**Priority:** P1 - Medium (Architecture Detection)
**Estimated Effort:** 2-3 hours
**Risk:** Low (Query-only change, no behavior modification unless UMA optimization enabled)
**Impact:** Enables future UMA-specific optimizations; improves diagnostic logging

**Notes:**
- Step 4 (UploadRingBuffer optimization) is optional and can be done as separate follow-up task
- Focus on Steps 1-3 first to establish architecture detection infrastructure
- UMA optimization provides ~10-20% upload performance improvement on integrated GPUs

---

## References

- https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
- https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_architecture
- https://learn.microsoft.com/windows/win32/direct3d12/memory-management
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MemoryManagement
- https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_memory_pool

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
