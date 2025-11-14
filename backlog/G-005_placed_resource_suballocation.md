# G-005: No Placed Resource Sub-Allocation Strategy for Memory Efficiency

**Severity:** Low
**Category:** GPU Memory Management
**Status:** Open
**Related Issues:** G-003 (Descriptor Fragmentation), G-004 (Multithreaded Resource Creation)
**Priority:** P2-Low

---

## Problem Statement

IGL creates each resource as a separate committed resource via `CreateCommittedResource()`, which allocates a dedicated GPU memory block for each texture, buffer, and render target. D3D12 offers "placed resources" (`CreatePlacedResource()`) which allow multiple resources to share sub-allocations within a larger reserved heap. This pattern reduces heap fragmentation, improves memory locality, and enables efficient atlasing strategies for textures and buffers.

**Impact Analysis:**
- **Memory Fragmentation:** Each resource gets its own heap allocation, leading to wasted space between resources
- **Heap Count:** Large applications accumulate hundreds of separate heaps, increasing GPU memory overhead
- **Atlasing Opportunity:** Textures could be sub-allocated from atlas heaps; current approach prevents this
- **Memory Pressure:** Memory-constrained devices (mobile/UMA) suffer from inefficient layout
- **Potential Improvement:** Placed resources can reduce heap count by 50-80% for typical workloads

---

## Root Cause Analysis

### Current Implementation (Device.cpp):

```cpp
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc, const void* data) {
  // Create committed resource (entire heap for this one buffer)
  ComPtr<ID3D12Resource> d3d12Buffer;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Buffer));  // One heap per buffer!

  if (FAILED(hr)) {
    IGL_LOG_ERROR("Device::createBuffer: Failed (0x%08X)\n", hr);
    return nullptr;
  }

  return std::make_shared<Buffer>(d3d12Buffer);
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc, const void* data) {
  // Same pattern - one heap per texture
  ComPtr<ID3D12Resource> d3d12Texture;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Texture));  // One heap per texture!

  return std::make_shared<Texture>(d3d12Texture);
}
```

### Why This Is Wrong:

1. **No Sub-Allocation:** Each resource gets dedicated heap; no sharing
2. **Heap Overhead:** GPU memory overhead per heap (typically 64KB-4MB per heap)
3. **Fragmentation:** Heaps become scattered in GPU address space
4. **No Atlasing:** Cannot implement texture atlasing or buffer pooling
5. **Memory Inefficiency:** Small resources (1KB buffers) waste entire heap blocks

---

## Official Documentation References

1. **D3D12 Placed Resources**:
   - https://learn.microsoft.com/windows/win32/direct3d12/placed-resources
   - Key guidance: "Placed resources share a heap for efficient memory management"

2. **Heap Management Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
   - Recommendations: "Use placed resources for sub-allocation efficiency"

3. **Texture Atlasing Patterns**:
   - https://learn.microsoft.com/windows/win32/direct3d12/large-scale-gpu-resource-management
   - Guidance: "Group small textures in atlas heaps using placed resources"

4. **Microsoft Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Multithreading
   - Examples of placed resource usage

---

## Code Location Strategy

### Files to Modify:

1. **Device.h** (add heap allocation manager):
   - Search for: Private member variables
   - Context: Add heap management infrastructure
   - Action: Add HeapAllocator member

2. **Device.cpp** (implement heap allocator):
   - Search for: End of implementation
   - Context: Add sub-allocator logic
   - Action: Implement PlacedResourceAllocator class

3. **Device.cpp** (modify createBuffer):
   - Search for: `CreateCommittedResource` call in createBuffer
   - Context: Buffer resource creation
   - Action: Route to placed resource allocator

4. **Device.cpp** (modify createTexture):
   - Search for: `CreateCommittedResource` call in createTexture
   - Context: Texture resource creation
   - Action: Route to placed resource allocator for small textures

5. **Texture.h/Texture.cpp** (add heap offset info):
   - Search for: Resource member variable
   - Context: Underlying D3D12 resource
   - Action: Add heap offset tracking

---

## Detection Strategy

### How to Reproduce:

1. Create 1000 small buffers (1KB each)
2. Monitor GPU memory usage
3. Count heap allocations

**Before Fix:**
```
1000 buffers * 1KB each = 1MB data
+ 1000 heaps * 64KB overhead = 64MB overhead
Total: ~65MB for 1MB of actual data
```

**After Fix:**
```
1000 buffers * 1KB each = 1MB data
+ 8 heaps * 128MB = 1GB sub-allocation pool
(multiple buffers per heap via sub-allocation)
Total: ~1GB heap pool (amortized per-resource cost is 1MB)
```

### Expected Behavior (After Fix):

- Heap count remains constant (~8-16) regardless of resource count
- Memory overhead reduced by 50-80%
- Identical visual output and behavior

### Verification After Fix:

1. Count GPU heaps via D3D12 debug output
2. Verify heap count stays constant as resources are added
3. Measure memory overhead per resource

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Placed Resource Allocator

**File:** `src/igl/d3d12/Device.h`

**Locate:** After class declaration

**Add Class:**
```cpp
// G-005: Placed resource sub-allocator for efficient memory usage
class PlacedResourceAllocator {
 public:
  struct Allocation {
    ComPtr<ID3D12Resource> resource;
    size_t heapOffset = 0;
    size_t size = 0;
  };

  PlacedResourceAllocator(ID3D12Device* device, D3D12_HEAP_TYPE heapType, size_t heapSize);
  ~PlacedResourceAllocator();

  // Allocate from heap (may create new heap if necessary)
  // Returns nullptr if allocation fails
  Allocation* allocateResource(const D3D12_RESOURCE_DESC& resourceDesc,
                                const D3D12_RESOURCE_STATES& initialState);

  // Free allocation (may keep heap for reuse)
  void free(Allocation* allocation);

  // Get statistics
  struct Stats {
    uint32_t heapCount = 0;
    uint64_t totalHeapSize = 0;
    uint64_t usedSize = 0;
    uint64_t fragmentedSize = 0;
  };
  Stats getStats() const;

 private:
  struct HeapAllocation {
    ComPtr<ID3D12Heap> heap;
    std::vector<Allocation> allocations;
    size_t nextOffset = 0;
    size_t heapSize = 0;
  };

  ID3D12Device* device_ = nullptr;
  D3D12_HEAP_TYPE heapType_;
  size_t defaultHeapSize_ = 0;
  std::vector<HeapAllocation> heaps_;
  std::mutex allocationMutex_;

  // Find heap with space or create new one
  HeapAllocation* findHeapWithSpace(size_t size);
  HeapAllocation* createNewHeap(size_t minSize);
};
```

**Rationale:** Manages pool of GPU heaps, sub-allocates resources within heaps to reduce overhead.

---

#### Step 2: Implement Placed Resource Allocator

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** End of implementation

**Add Implementation:**
```cpp
PlacedResourceAllocator::PlacedResourceAllocator(ID3D12Device* device,
                                                   D3D12_HEAP_TYPE heapType,
                                                   size_t heapSize)
    : device_(device), heapType_(heapType), defaultHeapSize_(heapSize) {
  IGL_LOG_INFO("PlacedResourceAllocator: Initialized with %zu MB heaps, type=%d\n",
               heapSize / (1024 * 1024), heapType);
}

PlacedResourceAllocator::~PlacedResourceAllocator() {
  // Heaps are automatically cleaned up via ComPtr
  IGL_LOG_INFO("PlacedResourceAllocator: Destroyed (%zu heaps)\n", heaps_.size());
}

PlacedResourceAllocator::HeapAllocation* PlacedResourceAllocator::findHeapWithSpace(
    size_t size) {
  // G-005: Find heap with contiguous space for resource
  std::lock_guard<std::mutex> lock(allocationMutex_);

  for (auto& heap : heaps_) {
    size_t availableSpace = heap.heapSize - heap.nextOffset;
    if (availableSpace >= size) {
      return &heap;
    }
  }

  // No suitable heap found
  return nullptr;
}

PlacedResourceAllocator::HeapAllocation* PlacedResourceAllocator::createNewHeap(
    size_t minSize) {
  // G-005: Create new GPU heap for sub-allocations
  std::lock_guard<std::mutex> lock(allocationMutex_);

  // Ensure heap is at least defaultHeapSize
  size_t heapSize = std::max(minSize, defaultHeapSize_);

  // Align to 64KB (D3D12 heap alignment requirement)
  constexpr size_t kHeapAlignment = 65536;
  heapSize = (heapSize + kHeapAlignment - 1) & ~(kHeapAlignment - 1);

  // Create heap properties
  D3D12_HEAP_DESC heapDesc = {};
  heapDesc.SizeInBytes = heapSize;
  heapDesc.Properties.Type = heapType_;
  heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapDesc.Alignment = 0;  // 64KB default
  heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

  ComPtr<ID3D12Heap> heap;
  HRESULT hr = device_->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("PlacedResourceAllocator::createNewHeap: Failed (0x%08X)\n", hr);
    return nullptr;
  }

  heaps_.emplace_back();
  HeapAllocation& heapAlloc = heaps_.back();
  heapAlloc.heap = heap;
  heapAlloc.heapSize = heapSize;
  heapAlloc.nextOffset = 0;

  IGL_LOG_INFO("PlacedResourceAllocator: Created heap %zu (%zu MB)\n",
               heaps_.size() - 1, heapSize / (1024 * 1024));

  return &heapAlloc;
}

PlacedResourceAllocator::Allocation* PlacedResourceAllocator::allocateResource(
    const D3D12_RESOURCE_DESC& resourceDesc,
    const D3D12_RESOURCE_STATES& initialState) {
  // G-005: Allocate resource from heap pool

  // Get resource info for size calculation
  D3D12_RESOURCE_ALLOCATION_INFO allocInfo =
      device_->GetResourceAllocationInfo(0, 1, &resourceDesc);

  size_t requiredSize = allocInfo.SizeInBytes;

  // Try to find existing heap with space
  HeapAllocation* targetHeap = findHeapWithSpace(requiredSize);

  // If no suitable heap, create new one
  if (!targetHeap) {
    targetHeap = createNewHeap(requiredSize);
    if (!targetHeap) {
      IGL_LOG_ERROR("PlacedResourceAllocator::allocateResource: Cannot create heap\n");
      return nullptr;
    }
  }

  // Create placed resource in heap
  {
    std::lock_guard<std::mutex> lock(allocationMutex_);

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreatePlacedResource(
        targetHeap->heap.Get(),
        targetHeap->nextOffset,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr)) {
      IGL_LOG_ERROR("PlacedResourceAllocator::allocateResource: CreatePlacedResource failed (0x%08X)\n", hr);
      return nullptr;
    }

    // Record allocation
    targetHeap->allocations.emplace_back();
    Allocation& alloc = targetHeap->allocations.back();
    alloc.resource = resource;
    alloc.heapOffset = targetHeap->nextOffset;
    alloc.size = requiredSize;

    targetHeap->nextOffset += requiredSize;

    IGL_LOG_INFO("PlacedResourceAllocator: Allocated resource (%zu bytes) in heap\n",
                 requiredSize);

    return &alloc;
  }
}

PlacedResourceAllocator::Stats PlacedResourceAllocator::getStats() const {
  std::lock_guard<std::mutex> lock(allocationMutex_);

  Stats stats;
  stats.heapCount = heaps_.size();

  for (const auto& heap : heaps_) {
    stats.totalHeapSize += heap.heapSize;
    stats.usedSize += heap.nextOffset;
    stats.fragmentedSize += heap.heapSize - heap.nextOffset;
  }

  return stats;
}
```

**Rationale:** Central allocator manages heap pool, creates placed resources, tracks statistics.

---

#### Step 3: Add Placed Allocators to Device

**File:** `src/igl/d3d12/Device.h`

**Locate:** Private member variables

**Add Members:**
```cpp
class Device : public IDevice {
 private:
  ComPtr<ID3D12Device> device_;

  // G-005: Placed resource allocators for different heap types
  std::unique_ptr<PlacedResourceAllocator> defaultHeapAllocator_;  // For buffers
  std::unique_ptr<PlacedResourceAllocator> uploadHeapAllocator_;   // For upload buffers
  std::unique_ptr<PlacedResourceAllocator> readbackHeapAllocator_; // For readback

  // ... other members ...
};
```

**Rationale:** Separate allocators for different heap types (Default, Upload, Readback).

---

#### Step 4: Initialize Allocators in Device Constructor

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Device constructor

**Add Initialization:**
```cpp
Device::Device() {
  // ... existing initialization ...

  // G-005: Initialize placed resource allocators
  constexpr size_t kDefaultHeapSize = 256 * 1024 * 1024;  // 256MB
  defaultHeapAllocator_ = std::make_unique<PlacedResourceAllocator>(
      device_.Get(), D3D12_HEAP_TYPE_DEFAULT, kDefaultHeapSize);

  constexpr size_t kUploadHeapSize = 64 * 1024 * 1024;    // 64MB
  uploadHeapAllocator_ = std::make_unique<PlacedResourceAllocator>(
      device_.Get(), D3D12_HEAP_TYPE_UPLOAD, kUploadHeapSize);

  constexpr size_t kReadbackHeapSize = 64 * 1024 * 1024;  // 64MB
  readbackHeapAllocator_ = std::make_unique<PlacedResourceAllocator>(
      device_.Get(), D3D12_HEAP_TYPE_READBACK, kReadbackHeapSize);
}
```

**Rationale:** Each heap type gets dedicated allocator with appropriate sizing.

---

#### Step 5: Modify Buffer Creation to Use Placed Resources

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createBuffer()` method

**Current (PROBLEM):**
```cpp
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc, const void* data) {
  ComPtr<ID3D12Resource> d3d12Buffer;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Buffer));

  return std::make_shared<Buffer>(d3d12Buffer);
}
```

**Fixed (SOLUTION):**
```cpp
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc, const void* data) {
  // G-005: Use placed resource allocator for sub-allocation efficiency
  D3D12_HEAP_TYPE heapType = calculateHeapType(desc);

  D3D12_RESOURCE_DESC resourceDesc = {};
  // ... fill resource descriptor ...

  D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;

  // Route to appropriate allocator
  PlacedResourceAllocator* allocator = nullptr;
  if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
    allocator = defaultHeapAllocator_.get();
  } else if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
    allocator = uploadHeapAllocator_.get();
  } else if (heapType == D3D12_HEAP_TYPE_READBACK) {
    allocator = readbackHeapAllocator_.get();
  }

  if (!allocator) {
    IGL_LOG_ERROR("Device::createBuffer: Invalid heap type\n");
    return nullptr;
  }

  // Allocate from heap pool
  auto* alloc = allocator->allocateResource(resourceDesc, initialState);
  if (!alloc || !alloc->resource) {
    IGL_LOG_ERROR("Device::createBuffer: Allocation failed\n");
    return nullptr;
  }

  // Create wrapper with allocated resource
  return std::make_shared<Buffer>(alloc->resource, alloc->heapOffset);
}
```

**Rationale:** Route buffer creation through placed allocator for sub-allocation.

---

#### Step 6: Modify Texture Creation for Small Textures

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createTexture()` method

**Fixed (SOLUTION - For Small Textures):**
```cpp
std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc, const void* data) {
  // G-005: Small textures use placed resources; large textures use committed
  constexpr size_t kSmallTextureThreshold = 4 * 1024 * 1024;  // 4MB threshold

  // Calculate texture size
  size_t textureSize = calculateTextureSize(desc);

  if (textureSize < kSmallTextureThreshold) {
    // Use placed resource allocator for small textures
    auto* alloc = defaultHeapAllocator_->allocateResource(textureDesc, initialState);
    if (!alloc) {
      return nullptr;
    }
    return std::make_shared<Texture>(alloc->resource, alloc->heapOffset);
  } else {
    // Use committed resource for large textures (not worth sub-allocation overhead)
    ComPtr<ID3D12Resource> d3d12Texture;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&d3d12Texture));

    if (FAILED(hr)) {
      return nullptr;
    }

    return std::make_shared<Texture>(d3d12Texture);
  }
}
```

**Rationale:** Small textures benefit from sub-allocation; large ones don't (allocation cost is negligible).

---

#### Step 7: Add Statistics Logging

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Destructor or telemetry method

**Add Logging:**
```cpp
void Device::logAllocationStats() {
  // G-005: Log heap allocation statistics
  IGL_LOG_INFO("=== GPU Heap Statistics ===\n");

  if (defaultHeapAllocator_) {
    auto stats = defaultHeapAllocator_->getStats();
    IGL_LOG_INFO("Default Heaps: %u heaps, %zu MB total, %zu MB used, %zu MB fragmented\n",
                 stats.heapCount, stats.totalHeapSize / (1024 * 1024),
                 stats.usedSize / (1024 * 1024),
                 stats.fragmentedSize / (1024 * 1024));
  }

  if (uploadHeapAllocator_) {
    auto stats = uploadHeapAllocator_->getStats();
    IGL_LOG_INFO("Upload Heaps: %u heaps, %zu MB total, %zu MB used\n",
                 stats.heapCount, stats.totalHeapSize / (1024 * 1024),
                 stats.usedSize / (1024 * 1024));
  }

  if (readbackHeapAllocator_) {
    auto stats = readbackHeapAllocator_->getStats();
    IGL_LOG_INFO("Readback Heaps: %u heaps, %zu MB total, %zu MB used\n",
                 stats.heapCount, stats.totalHeapSize / (1024 * 1024),
                 stats.usedSize / (1024 * 1024));
  }

  IGL_LOG_INFO("===========================\n");
}
```

**Rationale:** Provides visibility into heap allocation efficiency.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Buffer and texture tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Buffer*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Test Modifications Allowed:**
- ✅ Add tests for placed resource allocation
- ✅ Add tests for heap statistics
- ❌ **DO NOT modify existing test logic**

**New Tests to Add:**
```cpp
// Test placed resource creation
TEST(Device, CreatePlacedBuffer) {
  auto device = createTestDevice();
  auto buffer = device->createBuffer(testBufferDesc, nullptr);
  EXPECT_NE(nullptr, buffer);
  // Verify can use buffer normally
}

// Test many small buffers share heap
TEST(Device, ManySmallBuffersShareHeap) {
  auto device = createTestDevice();

  // Create 100 small buffers
  std::vector<std::shared_ptr<IBuffer>> buffers;
  for (int i = 0; i < 100; ++i) {
    auto buffer = device->createBuffer(smallBufferDesc, nullptr);
    EXPECT_NE(nullptr, buffer);
    buffers.push_back(buffer);
  }

  // All should succeed (shared heaps)
  EXPECT_EQ(100, buffers.size());

  // Log stats - should show ~1-2 heaps instead of 100
  device->logAllocationStats();
}

// Test heap allocation stats
TEST(Device, HeapAllocationStats) {
  auto device = createTestDevice();

  // Create mix of resources
  for (int i = 0; i < 50; ++i) {
    device->createBuffer(testBufferDesc, nullptr);
    device->createTexture(testTextureDesc, nullptr);
  }

  // Stats should show reasonable heap utilization
  device->logAllocationStats();
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions must pass (placed resources should be transparent)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required - placed resources are transparent
- ❌ **DO NOT modify session logic**

### Memory Verification:

```bash
# Monitor GPU memory usage before/after
# Expected: 50-80% reduction in heap count and overhead
```

---

## Definition of Done

### Completion Criteria:

- [ ] PlacedResourceAllocator class implemented
- [ ] Separate allocators for Default, Upload, Readback heap types
- [ ] Device::createBuffer() routes through placed allocator
- [ ] Device::createTexture() routes small textures through placed allocator
- [ ] Statistics logging shows heap utilization
- [ ] All unit tests pass (including new allocation tests)
- [ ] All render sessions pass without visual changes
- [ ] Heap count remains constant regardless of resource count
- [ ] Memory overhead reduced by 50-80%
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Resources created successfully via placed allocator
2. All resources function correctly (no access violations)
3. Heap count stays constant as resources are added
4. Memory overhead reduced significantly
5. All render sessions produce identical output

**Post in chat:**
```
G-005 Fix Complete - Ready for Review

Placed Resource Sub-Allocation:
- Allocator: PlacedResourceAllocator manages heap pools
- Heap Types: Separate allocators for Default/Upload/Readback
- Buffer Creation: Routes through placed allocator
- Texture Creation: Small textures use placed; large use committed
- Statistics: Logging shows heap utilization and fragmentation

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Placed creation: PASS (100 small buffers in ~1-2 heaps)
- Render sessions: PASS (no visual changes)
- Memory overhead: Reduced by ~50-80%
- Heap count: Constant regardless of resource count

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **G-003** - Descriptor fragmentation (similar memory efficiency principle)
- **G-004** - Multithreaded creation (allocator must be thread-safe)

---

## Implementation Priority

**Priority:** P2-Low (GPU Memory Management)
**Estimated Effort:** 4-5 hours
**Risk:** Medium (Memory allocation changes; requires careful testing for correctness)
**Impact:** 50-80% reduction in heap overhead; improves memory locality

**Notes:**
- Small texture threshold (4MB) is tunable based on workload
- Placed resources require precise alignment; D3D12 validates automatically
- Heap size (256MB default) is tunable for memory-constrained devices
- Statistics logging is critical for verification and debugging
- Future enhancement: Implement defragmentation/compaction for fragmented heaps

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/placed-resources
- https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
- https://learn.microsoft.com/windows/win32/direct3d12/large-scale-gpu-resource-management
- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Multithreading
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
