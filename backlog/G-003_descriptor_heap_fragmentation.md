# G-003: Descriptor Heap Fragmentation from No Reuse Strategy

**Severity:** Medium
**Category:** GPU Performance
**Status:** Open
**Related Issues:** A-006 (Heap Size Validation), C-002 (Double-Free Protection), G-001 (Barrier Batching)

---

## Problem Statement

The `DescriptorHeapManager` uses simple free lists for descriptor allocation but has no defragmentation or compaction strategy. As textures and samplers are created and destroyed, the descriptor heaps become fragmented with "holes" in the address space. While the free list tracks available indices, fragmentation can lead to inefficient heap utilization and performance degradation on descriptor-constrained systems.

**Impact Analysis:**
- **Heap Utilization:** Free list may show available descriptors, but they're scattered throughout heap, reducing spatial locality
- **Cache Efficiency:** GPU descriptor cache works best with contiguous descriptor ranges; fragmentation reduces cache hit rate
- **Allocation Failures:** Fragmented heap may fail to satisfy large contiguous allocation requests even when total free space is sufficient
- **Long-Term Degradation:** Extended application runtime causes increasing fragmentation without recovery

**The Danger:**
- Applications with dynamic resource creation/destruction accumulate fragmentation over time
- Descriptor cache thrashing from scattered descriptor access patterns
- Potential allocation failures in long-running applications (servers, persistent game instances)

---

## Root Cause Analysis

### Current Implementation (`DescriptorHeapManager.cpp:143-181`):

```cpp
uint32_t DescriptorHeapManager::allocateCbvSrvUav() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (freeCbvSrvUav_.empty()) {
    IGL_LOG_ERROR("DescriptorHeapManager: CBV/SRV/UAV heap exhausted!\n");
    return UINT32_MAX;
  }
  const uint32_t idx = freeCbvSrvUav_.back(); // Pop from back - no locality consideration
  freeCbvSrvUav_.pop_back();
  // ... high-watermark tracking ...
  return idx;
}

void DescriptorHeapManager::freeCbvSrvUav(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.cbvSrvUav) {
    freeCbvSrvUav_.push_back(index); // Simple push_back - no compaction
  }
}
```

**Free List Initialization (`DescriptorHeapManager.cpp:29-32`):**
```cpp
// Populate free list
freeCbvSrvUav_.reserve(sizes_.cbvSrvUav);
for (uint32_t i = 0; i < sizes_.cbvSrvUav; ++i) {
  freeCbvSrvUav_.push_back(i); // Sequential initialization
}
// Good initial layout, but no maintenance after allocations/frees
```

### Why This Is Wrong:

1. **No Compaction:** Free list grows unsorted as descriptors are freed; no periodic sorting to maintain locality
2. **Random Allocation Pattern:** Pop from back of free list returns last freed index, not necessarily best locality
3. **No Metrics:** No tracking of fragmentation level or average descriptor distance
4. **No Defragmentation Trigger:** No heuristic to determine when compaction would be beneficial

**Fragmentation Example:**
```
Initial: [0 1 2 3 4 5 6 7 8 9] (all free, contiguous)
Allocate 5 descriptors: Free list = [5 6 7 8 9]
Free indices 1, 3, 5: Free list = [5 6 7 8 9 1 3 5] (out of order!)
Allocate 3: Returns 5, 3, 1 (scattered across heap)
Result: Active descriptors at [0, 1, 2, 4, 6, 7, 8] (fragmented pattern)
```

---

## Official Documentation References

1. **Descriptor Heap Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps-overview#best-practices
   - Key guidance: "Group frequently used descriptors together for better cache locality"

2. **Descriptor Heap Performance**:
   - https://developer.nvidia.com/blog/bindless-rendering-dx12/
   - NVIDIA guidance: "Descriptor table locality affects GPU cache efficiency"

3. **Memory Management Patterns**:
   - https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
   - Guidance on compaction: "Periodic defragmentation improves long-term performance"

4. **DirectX-Graphics-Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12DynamicIndexing
   - Sample demonstrates descriptor indexing patterns

---

## Code Location Strategy

### Files to Modify:

1. **DescriptorHeapManager.h** (add fragmentation metrics):
   - Search for: High-watermark tracking member variables
   - Context: Add fragmentation tracking alongside usage metrics
   - Action: Add fragmentation ratio calculation methods

2. **DescriptorHeapManager.cpp** (`freeCbvSrvUav` method):
   - Search for: `freeCbvSrvUav_.push_back(index);`
   - Context: Freed descriptor added to free list
   - Action: Add sorted insertion to maintain free list order

3. **DescriptorHeapManager.cpp** (new compaction method):
   - Search for: End of implementation, before namespace close
   - Context: Add new defragmentation method
   - Action: Implement `defragment()` to sort free lists and log fragmentation

4. **DescriptorHeapManager.cpp** (`logUsageStats` method):
   - Search for: Existing telemetry logging
   - Context: Usage statistics logging
   - Action: Add fragmentation metrics to telemetry output

---

## Detection Strategy

### How to Reproduce:

Create fragmentation through allocation/free cycles:
```cpp
DescriptorHeapManager mgr;
mgr.initialize(device);

// Create fragmentation
std::vector<uint32_t> indices;
for (int i = 0; i < 20; ++i) {
  indices.push_back(mgr.allocateCbvSrvUav());
}

// Free every other index
for (int i = 0; i < 20; i += 2) {
  mgr.freeCbvSrvUav(indices[i]);
}

// Allocate again - indices will be scattered
for (int i = 0; i < 10; ++i) {
  uint32_t idx = mgr.allocateCbvSrvUav();
  // Without defrag: Returns indices in reverse free order (non-optimal)
  // With defrag: Returns lowest available indices (better locality)
}
```

### Verification After Fix:

1. Log free list contents after allocation/free cycles
2. Verify free list remains sorted (ascending order)
3. Check fragmentation ratio metric in telemetry
4. Measure descriptor cache hit rate (if driver provides metrics)

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Fragmentation Metrics

**File:** `src/igl/d3d12/DescriptorHeapManager.h`

**Locate:** Search for high-watermark member variables

**Current (PROBLEM):**
```cpp
class DescriptorHeapManager {
 private:
  // High-watermark tracking
  uint32_t highWaterMarkCbvSrvUav_ = 0;
  uint32_t highWaterMarkSamplers_ = 0;
  uint32_t highWaterMarkRtvs_ = 0;
  uint32_t highWaterMarkDsvs_ = 0;

  // No fragmentation metrics!
};
```

**Fixed (SOLUTION):**
```cpp
class DescriptorHeapManager {
 private:
  // High-watermark tracking
  uint32_t highWaterMarkCbvSrvUav_ = 0;
  uint32_t highWaterMarkSamplers_ = 0;
  uint32_t highWaterMarkRtvs_ = 0;
  uint32_t highWaterMarkDsvs_ = 0;

  // G-003: Fragmentation tracking
  // Calculates fragmentation ratio for a free list
  // Returns 0.0 (no fragmentation) to 1.0 (maximum fragmentation)
  float calculateFragmentation(const std::vector<uint32_t>& freeList,
                                uint32_t totalSize) const;

 public:
  // G-003: Defragment descriptor heaps by sorting free lists
  // Call periodically (e.g., per-frame or on demand) to maintain locality
  void defragment();
};
```

**Rationale:** Add metrics to quantify fragmentation and public defragment method for maintenance.

---

#### Step 2: Implement Fragmentation Calculation

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** End of file, before namespace close

**Add New Method:**
```cpp
float DescriptorHeapManager::calculateFragmentation(
    const std::vector<uint32_t>& freeList,
    uint32_t totalSize) const {
  // G-003: Calculate fragmentation ratio
  // Fragmentation metric: Average distance between adjacent free indices
  // normalized by heap size
  // 0.0 = perfectly contiguous, 1.0 = maximally scattered

  if (freeList.empty() || totalSize == 0) {
    return 0.0f;
  }

  if (freeList.size() == totalSize) {
    // All descriptors free - check if they're sorted
    bool sorted = true;
    for (size_t i = 1; i < freeList.size(); ++i) {
      if (freeList[i] < freeList[i - 1]) {
        sorted = false;
        break;
      }
    }
    return sorted ? 0.0f : 0.5f;
  }

  // Calculate average gap between free indices
  // First, sort a copy of the free list
  std::vector<uint32_t> sortedFree = freeList;
  std::sort(sortedFree.begin(), sortedFree.end());

  // Calculate sum of gaps between adjacent free indices
  uint64_t totalGap = 0;
  for (size_t i = 1; i < sortedFree.size(); ++i) {
    totalGap += (sortedFree[i] - sortedFree[i - 1]);
  }

  // Average gap between free indices
  const float avgGap = static_cast<float>(totalGap) / (sortedFree.size() - 1);

  // Normalize by heap size
  // Ideal gap = 1 (contiguous), maximum gap = totalSize
  // Fragmentation ratio = (avgGap - 1) / (totalSize - 1)
  const float fragmentation = (avgGap - 1.0f) / static_cast<float>(totalSize - 1);

  return std::min(1.0f, std::max(0.0f, fragmentation));
}
```

**Rationale:** Quantifies fragmentation as average distance between free descriptors. Low fragmentation = descriptors close together (good locality). High fragmentation = scattered descriptors (poor locality).

---

#### Step 3: Implement Defragmentation Method

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** After fragmentation calculation method

**Add New Method:**
```cpp
void DescriptorHeapManager::defragment() {
  // G-003: Defragment descriptor heaps by sorting free lists
  std::lock_guard<std::mutex> lock(mutex_);

  IGL_LOG_INFO("=== Descriptor Heap Defragmentation ===\n");

  // Defragment CBV/SRV/UAV heap
  if (!freeCbvSrvUav_.empty()) {
    const float fragBefore = calculateFragmentation(freeCbvSrvUav_, sizes_.cbvSrvUav);
    std::sort(freeCbvSrvUav_.begin(), freeCbvSrvUav_.end());
    const float fragAfter = calculateFragmentation(freeCbvSrvUav_, sizes_.cbvSrvUav);
    IGL_LOG_INFO("  CBV/SRV/UAV: Fragmentation %.2f%% -> %.2f%% (sorted %zu free indices)\n",
                 fragBefore * 100.0f, fragAfter * 100.0f, freeCbvSrvUav_.size());
  }

  // Defragment sampler heap
  if (!freeSamplers_.empty()) {
    const float fragBefore = calculateFragmentation(freeSamplers_, sizes_.samplers);
    std::sort(freeSamplers_.begin(), freeSamplers_.end());
    const float fragAfter = calculateFragmentation(freeSamplers_, sizes_.samplers);
    IGL_LOG_INFO("  Samplers:    Fragmentation %.2f%% -> %.2f%% (sorted %zu free indices)\n",
                 fragBefore * 100.0f, fragAfter * 100.0f, freeSamplers_.size());
  }

  // Defragment RTV heap
  if (!freeRtvs_.empty()) {
    const float fragBefore = calculateFragmentation(freeRtvs_, sizes_.rtvs);
    std::sort(freeRtvs_.begin(), freeRtvs_.end());
    const float fragAfter = calculateFragmentation(freeRtvs_, sizes_.rtvs);
    IGL_LOG_INFO("  RTVs:        Fragmentation %.2f%% -> %.2f%% (sorted %zu free indices)\n",
                 fragBefore * 100.0f, fragAfter * 100.0f, freeRtvs_.size());
  }

  // Defragment DSV heap
  if (!freeDsvs_.empty()) {
    const float fragBefore = calculateFragmentation(freeDsvs_, sizes_.dsvs);
    std::sort(freeDsvs_.begin(), freeDsvs_.end());
    const float fragAfter = calculateFragmentation(freeDsvs_, sizes_.dsvs);
    IGL_LOG_INFO("  DSVs:        Fragmentation %.2f%% -> %.2f%% (sorted %zu free indices)\n",
                 fragBefore * 100.0f, fragAfter * 100.0f, freeDsvs_.size());
  }

  IGL_LOG_INFO("========================================\n");
}
```

**Rationale:** Sorts free lists to restore ascending order, improving allocation locality. Logs before/after fragmentation metrics for visibility.

---

#### Step 4: Maintain Sorted Free Lists During Free Operations

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** Search for `freeCbvSrvUav_.push_back(index);` in `freeCbvSrvUav` method

**Current (PROBLEM):**
```cpp
void DescriptorHeapManager::freeCbvSrvUav(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.cbvSrvUav) {
    freeCbvSrvUav_.push_back(index); // Appends to end - breaks sorting
  }
}
```

**Fixed (SOLUTION - Option 1: Lazy Approach):**
```cpp
void DescriptorHeapManager::freeCbvSrvUav(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.cbvSrvUav) {
    // G-003: Use push_back for fast free, rely on periodic defragment() to restore order
    freeCbvSrvUav_.push_back(index);

    // Optional: Auto-defragment if free list grows large and unsorted
    // This is a heuristic - tune threshold based on performance requirements
    constexpr size_t kDefragThreshold = 100; // Defrag every 100 frees
    if (freeCbvSrvUav_.size() > kDefragThreshold &&
        freeCbvSrvUav_.size() % kDefragThreshold == 0) {
      // Trigger defragmentation (just for this heap, not all)
      std::sort(freeCbvSrvUav_.begin(), freeCbvSrvUav_.end());
      IGL_LOG_INFO("DescriptorHeapManager: Auto-defragmented CBV/SRV/UAV free list (%zu indices)\n",
                   freeCbvSrvUav_.size());
    }
  }
}
```

**Fixed (SOLUTION - Option 2: Eager Sorted Insert):**
```cpp
void DescriptorHeapManager::freeCbvSrvUav(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.cbvSrvUav) {
    // G-003: Insert in sorted position to maintain free list order
    // This has O(n) cost per free, but keeps list sorted for optimal allocation
    auto it = std::lower_bound(freeCbvSrvUav_.begin(), freeCbvSrvUav_.end(), index);
    freeCbvSrvUav_.insert(it, index);
  }
}
```

**Recommendation:** Use **Option 1 (Lazy)** for free operations (push_back) and call `defragment()` periodically (e.g., per-frame or every N frames). This balances performance (fast free) with locality (periodic compaction).

**Apply same pattern to all free methods:**
```cpp
void DescriptorHeapManager::freeSampler(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.samplers) {
    freeSamplers_.push_back(index); // Fast append
    // Optional: Periodic auto-defrag
  }
}

void DescriptorHeapManager::freeRTV(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.rtvs) {
    freeRtvs_.push_back(index); // Fast append
    // Optional: Periodic auto-defrag
  }
}

void DescriptorHeapManager::freeDSV(uint32_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index != UINT32_MAX && index < sizes_.dsvs) {
    freeDsvs_.push_back(index); // Fast append
    // Optional: Periodic auto-defrag
  }
}
```

**Rationale:** Lazy approach keeps free operations fast (O(1) append) while periodic defragmentation restores locality. Eager sorted insert (O(n)) is acceptable for low-frequency free operations but may be too costly in high-churn scenarios.

---

#### Step 5: Add Fragmentation Metrics to Telemetry

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** Search for `logUsageStats` method

**Current (PROBLEM):**
```cpp
void DescriptorHeapManager::logUsageStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  IGL_LOG_INFO("=== Descriptor Heap Usage Statistics ===\n");

  // Logs usage percentages and high-watermarks
  // ... existing logging ...

  IGL_LOG_INFO("========================================\n");
  // No fragmentation metrics!
}
```

**Fixed (SOLUTION):**
```cpp
void DescriptorHeapManager::logUsageStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  IGL_LOG_INFO("=== Descriptor Heap Usage Statistics ===\n");

  // ... existing usage and high-watermark logging ...

  // G-003: Add fragmentation metrics
  IGL_LOG_INFO("\n");
  IGL_LOG_INFO("=== Fragmentation Metrics ===\n");

  const float cbvSrvUavFrag = calculateFragmentation(freeCbvSrvUav_, sizes_.cbvSrvUav);
  IGL_LOG_INFO("  CBV/SRV/UAV: %.2f%% fragmentation\n", cbvSrvUavFrag * 100.0f);

  const float samplerFrag = calculateFragmentation(freeSamplers_, sizes_.samplers);
  IGL_LOG_INFO("  Samplers:    %.2f%% fragmentation\n", samplerFrag * 100.0f);

  const float rtvFrag = calculateFragmentation(freeRtvs_, sizes_.rtvs);
  IGL_LOG_INFO("  RTVs:        %.2f%% fragmentation\n", rtvFrag * 100.0f);

  const float dsvFrag = calculateFragmentation(freeDsvs_, sizes_.dsvs);
  IGL_LOG_INFO("  DSVs:        %.2f%% fragmentation\n", dsvFrag * 100.0f);

  // Recommendation based on fragmentation level
  const float maxFrag = std::max({cbvSrvUavFrag, samplerFrag, rtvFrag, dsvFrag});
  if (maxFrag > 0.3f) {
    IGL_LOG_INFO("  Recommendation: Fragmentation >30%%, consider calling defragment()\n");
  }

  IGL_LOG_INFO("========================================\n");
}
```

**Rationale:** Adds fragmentation metrics to telemetry output, providing visibility into heap health and guidance on when defragmentation would be beneficial.

---

#### Step 6: Periodic Defragmentation Call (Optional Enhancement)

**File:** `src/igl/d3d12/Device.cpp` or application code

**Locate:** Per-frame update or periodic maintenance point

**Add Periodic Call:**
```cpp
// In application frame loop or Device per-frame maintenance
void Device::perFrameMaintenance() {
  // G-003: Periodic descriptor heap defragmentation
  // Call every N frames to maintain locality without excessive overhead
  static uint32_t frameCounter = 0;
  constexpr uint32_t kDefragInterval = 300; // Every 300 frames (~5 seconds at 60 FPS)

  if (++frameCounter >= kDefragInterval) {
    frameCounter = 0;

    // Get descriptor heap manager from context
    if (auto* heapMgr = ctx_->getDescriptorHeapManager()) {
      heapMgr->defragment();
    }
  }
}
```

**Rationale:** Periodic defragmentation (every 5 seconds) is sufficient for most applications. Tune interval based on resource churn rate.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Descriptor heap specific tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Descriptor*"
```

**Test Modifications Allowed:**
- ✅ Add test for fragmentation calculation
- ✅ Add test for defragmentation effectiveness
- ❌ **DO NOT modify cross-platform test logic**

**New Tests to Add:**
```cpp
// Test fragmentation detection
TEST(DescriptorHeapManager, DetectsFragmentation) {
  DescriptorHeapManager mgr;
  mgr.initialize(testDevice);

  // Create fragmentation
  std::vector<uint32_t> indices;
  for (int i = 0; i < 20; ++i) {
    indices.push_back(mgr.allocateCbvSrvUav());
  }

  // Free every other index
  for (int i = 0; i < 20; i += 2) {
    mgr.freeCbvSrvUav(indices[i]);
  }

  // Log stats should show non-zero fragmentation
  mgr.logUsageStats(); // Check output manually or instrument
}

// Test defragmentation reduces fragmentation
TEST(DescriptorHeapManager, DefragmentationImproveLocality) {
  DescriptorHeapManager mgr;
  mgr.initialize(testDevice);

  // Create fragmentation as above
  // ...

  // Defragment
  mgr.defragment();

  // Subsequent allocations should be more contiguous
  std::vector<uint32_t> newIndices;
  for (int i = 0; i < 10; ++i) {
    newIndices.push_back(mgr.allocateCbvSrvUav());
  }

  // Verify indices are sorted (ascending order)
  for (size_t i = 1; i < newIndices.size(); ++i) {
    EXPECT_LT(newIndices[i - 1], newIndices[i]);
  }
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass (defragmentation is transparent)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required - defragmentation should be transparent
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (existing + new fragmentation tests)
- [ ] Fragmentation calculation method implemented
- [ ] Defragmentation method implemented and sorts free lists
- [ ] Fragmentation metrics added to telemetry logging
- [ ] Optional: Periodic defragmentation call added
- [ ] All render sessions pass without visual changes
- [ ] Telemetry shows fragmentation metrics
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Telemetry logs show fragmentation metrics
2. Defragmentation reduces fragmentation percentage
3. All render sessions pass

**Post in chat:**
```
G-003 Fix Complete - Ready for Review

Descriptor Heap Defragmentation:
- Metrics: calculateFragmentation() quantifies heap health
- Defragmentation: defragment() sorts free lists for locality
- Telemetry: Fragmentation percentage shown in logUsageStats()
- Optional: Periodic defragmentation every N frames

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Fragmentation test: PASS (detects and reduces fragmentation)
- Render sessions: PASS (no visual changes)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **A-006** - Descriptor heap size validation (validates heap capacity)
- **C-002** - Double-free protection (prevents corruption of free lists)
- **G-001** - Barrier batching (similar performance optimization principle)

---

## Implementation Priority

**Priority:** P1 - Medium (GPU Performance)
**Estimated Effort:** 3-4 hours
**Risk:** Low (Defragmentation is transparent; free list sorting has no functional impact)
**Impact:** Improves descriptor cache locality; prevents long-term degradation in dynamic applications

**Notes:**
- Fragmentation calculation provides diagnostic value even without defragmentation
- Defragmentation is most beneficial for long-running applications with high resource churn
- Lazy defragmentation (periodic) is recommended over eager sorted insert for performance
- Consider making defragmentation interval configurable based on application needs

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps-overview#best-practices
- https://developer.nvidia.com/blog/bindless-rendering-dx12/
- https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12DynamicIndexing
- https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12descriptorheap

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
