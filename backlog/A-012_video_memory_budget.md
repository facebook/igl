# A-012: Video Memory Budget Not Tracked

**Priority**: LOW
**Category**: Architecture - Resource Management
**Estimated Effort**: 5-6 hours
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend does not track or expose video memory budgets and constraints. Modern graphics systems (especially on mobile and UMA architectures) have specific memory budgets for GPU-accessible memory, but the IGL D3D12 backend provides no mechanism to:

1. **Query Available Memory**: Determine how much GPU memory is available and allocable
2. **Track Usage**: Monitor current memory usage by the application
3. **Enforce Budgets**: Prevent out-of-memory errors by enforcing per-frame or per-resource budgets
4. **Query Adapter Memory**: Determine dedicated vs. shared memory for each adapter
5. **Graceful Degradation**: Reduce memory usage when approaching limits

**Symptoms**:
- Application cannot determine if a texture can be created within budget
- Out-of-memory crashes when allocating large resources
- No visibility into GPU memory usage
- Unnecessary memory allocations when memory is limited
- No ability to reduce quality/resolution when memory is constrained
- Performance degradation from memory pressure (paging to system RAM)
- Inconsistent behavior across different hardware (integrated vs. discrete GPUs)

**Impact**:
- Unpredictable memory usage on different hardware
- Application crashes on low-memory systems
- Cannot optimize for memory-constrained devices
- Poor user experience on integrated GPU systems
- No way to implement memory-efficient streaming
- Lost opportunity for dynamic quality adjustment based on memory availability

---

## 2. Root Cause Analysis

### 2.1 Current Memory Status

The D3D12 backend does not implement memory budget tracking. Resource creation is not validated against any memory budget constraints.

**File**: `src/igl/d3d12/Device.cpp` and related resource creation files

**Current Approach**:
- Resources are created with no memory budget checks
- No tracking of allocated memory
- No reporting of available memory
- No enforcement of memory limits

### 2.2 Why This Is Wrong

**Problem 1**: D3D12 devices don't directly expose total memory, but Windows can query it via DXGI or WDDM (Windows Display Driver Model). This information is not being queried or exposed.

**Problem 2**: GPU memory management is different on:
- Dedicated GPUs: Have their own memory pool (VRAM)
- Integrated GPUs: Share system RAM (UMA architecture)
- Remote GPUs: Network bandwidth limits
But IGL treats all the same with no differentiation.

**Problem 3**: Memory budget is not enforced per-frame or per-application. Applications using IGL have no way to stay within safe memory bounds.

**Problem 4**: No distinction between:
- Dedicated GPU memory (VRAM) - fast, limited
- Shared memory (System RAM) - larger, slower
- Upload buffer memory - temporary, should be recycled

**Problem 5**: No exposure of:
- Total available memory
- Current usage by application
- Peak memory usage
- Memory pressure indicators
- Memory allocation failures

---

## 3. Official Documentation References

**Primary Resources**:

1. **DXGI Adapter Description**:
   - https://learn.microsoft.com/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1
   - Contains SharedSystemMemory and DedicatedVideoMemory fields

2. **DXGI Memory Residency**:
   - https://learn.microsoft.com/windows/win32/direct3d12/residency
   - Memory residency and budget management for D3D12

3. **Query Device Video Memory**:
   - https://learn.microsoft.com/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgidevice3-queryresourceresidency
   - Query resource residency status

4. **Process Memory Information**:
   - https://learn.microsoft.com/windows/win32/procthread/working-with-processes
   - System memory querying capabilities

5. **WDDM Memory Management**:
   - https://learn.microsoft.com/windows/win32/direct3d12/residency
   - Windows memory management for graphics

6. **Best Practices**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12MemoryManagement
   - Memory management patterns (if sample exists)

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for adapter memory queries**:
```
Pattern: "SharedSystemMemory" OR "DedicatedVideoMemory" OR "GetDesc1"
Files: src/igl/d3d12/D3D12Context.cpp
Focus: Where adapter memory should be queried
```

**Search for resource allocation**:
```
Pattern: "CreateResource" OR "CreateHeap" OR "CreateBuffer" OR "CreateTexture"
Files: src/igl/d3d12/Buffer.cpp, src/igl/d3d12/Texture.cpp
Focus: Where resources are created without memory checks
```

**Search for memory tracking**:
```
Pattern: "memory" OR "alloc" OR "residency"
Files: src/igl/d3d12/*.cpp
Focus: Existing memory management code
```

**Search for device capabilities**:
```
Pattern: "validateDeviceLimits" OR "getDeviceInfo"
Files: src/igl/d3d12/Device.cpp
Focus: Where device info is exposed
```

### 4.2 File Locations

- `src/igl/d3d12/D3D12Context.cpp` - Where adapter memory should be queried
- `src/igl/d3d12/Device.cpp` - Device capability exposure
- `src/igl/d3d12/Device.h` - Public interface
- `src/igl/d3d12/Buffer.cpp` - Buffer allocation
- `src/igl/d3d12/Texture.cpp` - Texture allocation
- `src/igl/d3d12/UploadRingBuffer.cpp` - Temporary memory tracking

### 4.3 Key Code Patterns

Look for:
- `DXGI_ADAPTER_DESC1` queries
- `CreateResource` or `CreateHeap` calls
- Resource size calculations
- Device memory querying

---

## 5. Detection Strategy

### 5.1 How to Reproduce

**Scenario 1: Check memory reporting**
```
1. Create device and check if memory info is available
2. Try to find method to query available GPU memory
3. Expected: Should return adapter memory information
4. Current: No such interface exists
```

**Scenario 2: Allocate large texture**
```
1. Allocate a very large texture (e.g., 4K at 32-bit)
2. Try allocate another one
3. Expected: Should warn or fail gracefully when approaching limits
4. Current: May crash with out-of-memory error
```

**Scenario 3: Monitor memory usage**
```
1. Create multiple resources
2. Try to monitor total memory usage
3. Expected: Should be able to query current usage
4. Current: No tracking available
```

### 5.2 Verification After Fix

1. **Memory Reporting**: Query available and used memory
2. **Budget Enforcement**: Prevent allocations exceeding limits
3. **Memory Tracking**: Verify allocation/deallocation tracking
4. **No False Limits**: Ensure limits don't prevent valid operations
5. **Multi-Adapter Support**: Correct memory reporting for each adapter

---

## 6. Fix Guidance

### 6.1 Step-by-Step Implementation

#### Step 1: Create Memory Budget Structure and Tracking

**File**: `src/igl/d3d12/Device.h`

**Locate**: After class member variables declaration

**Current (PROBLEM)**:
```cpp
// No memory budget tracking
// Resources created without any memory constraints
```

**Fixed (SOLUTION)**:
```cpp
// Memory budget and tracking structure
struct MemoryBudget {
    /// Total dedicated GPU memory (bytes)
    uint64_t dedicatedVideoMemory = 0;

    /// Total shared system memory accessible to GPU (bytes)
    uint64_t sharedSystemMemory = 0;

    /// Total available memory (dedicated + shared)
    uint64_t totalAvailableMemory() const {
        return dedicatedVideoMemory + sharedSystemMemory;
    }

    /// Current estimated usage by this device (bytes)
    uint64_t estimatedUsage = 0;

    /// Maximum allowed usage by this application (optional soft limit)
    uint64_t userDefinedBudgetLimit = 0;

    /// Percentage of memory currently used
    double getUsagePercentage() const {
        if (totalAvailableMemory() == 0) return 0.0;
        return (static_cast<double>(estimatedUsage) / totalAvailableMemory()) * 100.0;
    }

    /// Check if memory allocation would exceed soft limit
    bool wouldExceedBudget(uint64_t allocationSize) const {
        if (userDefinedBudgetLimit == 0) return false;  // No limit set
        return (estimatedUsage + allocationSize) > userDefinedBudgetLimit;
    }

    /// Check if memory is critically low (>90% used)
    bool isMemoryCritical() const {
        return getUsagePercentage() > 90.0;
    }

    /// Check if memory is running low (>70% used)
    bool isMemoryLow() const {
        return getUsagePercentage() > 70.0;
    }
};

// In Device class private section:
private:
    MemoryBudget memoryBudget_;
    std::mutex memoryTrackingMutex_;

// In Device class public section:
public:
    /// Get current memory budget information
    /// @return Memory budget and usage information
    MemoryBudget getMemoryBudget() const {
        std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
        return memoryBudget_;
    }

    /// Set a soft memory budget limit (optional)
    /// @param limitBytes Maximum memory to allow allocation (0 = no limit)
    void setMemoryBudgetLimit(uint64_t limitBytes) {
        std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
        memoryBudget_.userDefinedBudgetLimit = limitBytes;
        IGL_LOG_INFO("Device: Memory budget limit set to %.2f MB\n",
                     limitBytes / (1024.0 * 1024.0));
    }

    /// Get memory usage percentage
    /// @return Percentage of total memory currently used
    double getMemoryUsagePercentage() const {
        std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
        return memoryBudget_.getUsagePercentage();
    }

    /// Check if memory is running low
    /// @return true if usage > 70%
    bool isMemoryLow() const {
        std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
        return memoryBudget_.isMemoryLow();
    }

    /// Check if memory is critical
    /// @return true if usage > 90%
    bool isMemoryCritical() const {
        std::lock_guard<std::mutex> lock(memoryTrackingMutex_);
        return memoryBudget_.isMemoryCritical();
    }

    /// Update estimated memory usage (called by resource creators)
    /// @param delta Change in memory usage (positive for allocation, negative for deallocation)
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
```

**Rationale**:
- Provides centralized memory budget tracking
- Thread-safe memory usage tracking
- Multiple query levels (total, usage percentage, critical thresholds)
- Supports optional soft memory limits
- Easy to extend with more sophisticated memory management

#### Step 2: Query Adapter Memory at Device Creation

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: After device creation, in createDevice() method

**Current (PROBLEM)**:
```cpp
// Device created but no memory budget queried
// createDevice() ends without memory information
```

**Fixed (SOLUTION)**:
```cpp
// After successful device creation (around line 425-430), add:

// Query adapter memory budget
Microsoft::WRL::ComPtr<IDXGIAdapter1> selectedAdapter;
if (factory6.Get() && cand.Get()) {
    selectedAdapter = cand;
} else if (adapter_.Get()) {
    selectedAdapter = adapter_;
} else {
    // WARP adapter - get handle for memory query
    dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(selectedAdapter.GetAddressOf()));
}

if (selectedAdapter.Get()) {
    DXGI_ADAPTER_DESC1 adapterDesc = {};
    selectedAdapter->GetDesc1(&adapterDesc);

    // Store memory information
    // Note: DedicatedSystemMemory is for resource creation (on-board memory)
    // SharedSystemMemory is system RAM accessible to GPU
    memoryBudget_.dedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
    memoryBudget_.sharedSystemMemory = adapterDesc.SharedSystemMemory;

    IGL_LOG_INFO("D3D12Context: GPU Memory Budget\n");
    IGL_LOG_INFO("  Dedicated Video Memory: %.2f MB\n",
                 adapterDesc.DedicatedVideoMemory / (1024.0 * 1024.0));
    IGL_LOG_INFO("  Shared System Memory: %.2f MB\n",
                 adapterDesc.SharedSystemMemory / (1024.0 * 1024.0));
    IGL_LOG_INFO("  Total Available: %.2f MB\n",
                 memoryBudget_.totalAvailableMemory() / (1024.0 * 1024.0));

    // Recommend conservative budget (80% of available)
    uint64_t recommendedBudget = static_cast<uint64_t>(
        memoryBudget_.totalAvailableMemory() * 0.8);
    IGL_LOG_INFO("  Recommended Budget (80%%): %.2f MB\n",
                 recommendedBudget / (1024.0 * 1024.0));
}
```

**Rationale**:
- Queries actual adapter memory at device creation
- Logs memory information for debugging and validation
- Provides recommendations for safe memory budgets
- Makes adapter memory available for rest of device lifetime

#### Step 3: Add Memory Tracking to Resource Creation

**File**: `src/igl/d3d12/Buffer.cpp`

**Locate**: createBuffer() or CreateResource() call

**Current (PROBLEM)**:
```cpp
// Buffer created without memory tracking
HRESULT hr = device->CreateCommittedResource(
    &heapProperties,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_COMMON,
    nullptr,
    IID_PPV_ARGS(bufferResource_.GetAddressOf()));
// No memory update, no budget check
```

**Fixed (SOLUTION)**:
```cpp
// Before resource creation, check budget
uint64_t bufferSize = desc.size;
if (device.getMemoryBudget().wouldExceedBudget(bufferSize)) {
    IGL_LOG_WARNING("Buffer::Buffer: Allocation would exceed budget (%.2f MB > limit)\n",
                    bufferSize / (1024.0 * 1024.0));
    // Continue anyway (warning only), but log clearly
}

// Create resource
HRESULT hr = device->CreateCommittedResource(
    &heapProperties,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    D3D12_RESOURCE_STATE_COMMON,
    nullptr,
    IID_PPV_ARGS(bufferResource_.GetAddressOf()));

if (SUCCEEDED(hr)) {
    // Update memory tracking
    device.updateMemoryUsage(static_cast<int64_t>(bufferSize));

    IGL_LOG_INFO("Buffer::Buffer: Created buffer (%.2f MB, total usage: %.1f%%)\n",
                 bufferSize / (1024.0 * 1024.0),
                 device.getMemoryUsagePercentage());

    // Warn if approaching limits
    if (device.isMemoryCritical()) {
        IGL_LOG_WARNING("Buffer::Buffer: GPU memory CRITICAL (%.1f%% used)\n",
                        device.getMemoryUsagePercentage());
    } else if (device.isMemoryLow()) {
        IGL_LOG_WARNING("Buffer::Buffer: GPU memory LOW (%.1f%% used)\n",
                        device.getMemoryUsagePercentage());
    }
}
```

**Rationale**:
- Tracks memory usage at allocation time
- Warns before budget is exceeded
- Provides real-time memory pressure information
- Enables application to make quality decisions

#### Step 4: Update Resource Destruction

**File**: `src/igl/d3d12/Buffer.cpp`

**Locate**: Destructor or Resource release

**Current (PROBLEM)**:
```cpp
Buffer::~Buffer() {
    bufferResource_.Reset();
    // No memory deallocation tracking
}
```

**Fixed (SOLUTION)**:
```cpp
Buffer::~Buffer() {
    if (bufferResource_.Get() && device_) {
        // Update memory tracking
        device_->updateMemoryUsage(-static_cast<int64_t>(size_));

        IGL_LOG_INFO("Buffer::~Buffer: Released buffer (%.2f MB, new usage: %.1f%%)\n",
                     size_ / (1024.0 * 1024.0),
                     device_->getMemoryUsagePercentage());
    }
    bufferResource_.Reset();
}
```

**Rationale**:
- Balances allocation with deallocation tracking
- Accurate memory usage statistics
- Tracks when pressure is relieved

#### Step 5: Similar Updates for Texture Resources

**File**: `src/igl/d3d12/Texture.cpp`

**Locate**: Texture creation and destruction

**Current (PROBLEM)**:
```cpp
// Similar to Buffer - no memory tracking for textures
```

**Fixed (SOLUTION)**: Apply same pattern as Buffer (Step 3 and 4)

**Rationale**: Textures often consume more memory than buffers, so tracking is critical

---

## 7. Testing Requirements

### 7.1 Unit Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Run resource allocation tests
./build/Debug/IGLTests.exe --gtest_filter="*Buffer*"
./build/Debug/IGLTests.exe --gtest_filter="*Texture*"
./build/Debug/IGLTests.exe --gtest_filter="*Memory*"
```

**Test Modifications Allowed**:
- ✅ Add memory budget detection tests
- ✅ Add memory tracking tests
- ✅ Add budget limit enforcement tests
- ✅ Mock low memory scenarios
- ❌ **DO NOT modify cross-platform resource allocation logic**

### 7.2 Integration Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# All render sessions (should not change memory behavior)
./test_all_sessions.bat

# Sessions with heavy resource allocation
./build/Debug/RenderSessions.exe --session=Textured3DCube
./build/Debug/RenderSessions.exe --session=TextureArray
```

**Expected Results**:
- Sessions complete successfully
- Memory tracking shows reasonable values
- No out-of-memory errors
- Performance unchanged

### 7.3 Manual Verification

1. **Memory Reporting**:
   - Run application and check logged memory information
   - Verify totals match adapter specifications
   - Check usage percentage is reasonable

2. **Memory Tracking**:
   - Create multiple resources
   - Verify usage increases appropriately
   - Check usage decreases when resources are released

3. **Budget Warnings**:
   - Create large resources
   - Monitor for memory low/critical warnings
   - Verify warnings appear before actual out-of-memory

---

## 8. Definition of Done

### 8.1 Completion Criteria

- [ ] Memory budget structure created
- [ ] Adapter memory queried at device creation
- [ ] Memory tracking integrated in resource creation/destruction
- [ ] Public API for memory budget queries
- [ ] Memory usage tracking in Buffer and Texture
- [ ] Budget warnings logged at appropriate thresholds
- [ ] All unit tests pass
- [ ] All integration tests (render sessions) pass
- [ ] Memory reporting appears in logs
- [ ] Code review approved
- [ ] No performance regression

### 8.2 User Confirmation Required

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Memory budget is correctly reported (matches your GPU memory)
2. Memory tracking increases/decreases with resource creation/deletion
3. Warnings appear before running out of memory
4. Render sessions complete without errors

**Post in chat**:
```
A-012 Fix Complete - Ready for Review
- Memory budget detection: PASS (Queried from adapter)
- Memory tracking: PASS (Allocation/deallocation tracked)
- Budget warnings: PASS (Low/Critical thresholds working)
- Unit tests: PASS (All memory-related tests)
- Render sessions: PASS (All sessions complete successfully)
- Logging verification: PASS (Memory budget and usage shown)

Awaiting confirmation to proceed.
```

---

## 9. Related Issues

### 9.1 Blocks

- **A-013** - Background Shader Compilation (can use memory budget for compilation queue)

### 9.2 Related

- **A-002** - Adapter Selection (both query adapter capabilities)
- **A-003** - UMA Architecture Query (related to shared vs. dedicated memory)

---

## 10. Implementation Priority

**Priority**: P2 - Low (Resource Management)
**Estimated Effort**: 5-6 hours
**Risk**: Medium (touches resource creation paths, needs careful testing)
**Impact**: Enables memory-aware application design, prevents out-of-memory crashes on limited hardware, improves device compatibility

---

## 11. References

- https://learn.microsoft.com/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1
- https://learn.microsoft.com/windows/win32/direct3d12/residency
- https://learn.microsoft.com/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgidevice3-queryresourceresidency

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
