# G-012: CPU Descriptor Allocation Not Pooled

**Priority**: MEDIUM
**Category**: Graphics - Performance
**Estimated Effort**: 3-4 days
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend allocates CPU descriptors (for shader resource views, render target views, depth stencil views, samplers) individually without pooling or reuse, causing excessive heap allocations and potential fragmentation. Each descriptor heap has a limited size, and inefficient allocation can lead to running out of descriptors or creating too many heaps.

**Symptoms**:
- Frequent creation of new descriptor heaps
- Running out of descriptors in complex scenes
- High CPU overhead in descriptor allocation
- Memory fragmentation in descriptor heaps
- Descriptors never freed or reused
- Performance degradation over time

**Impact**:
- Increased CPU usage for descriptor management
- Memory waste from unused descriptors
- Potential crashes when descriptor limits are hit
- Poor scaling with number of textures and render targets
- Inefficient cache behavior due to fragmentation

---

## 2. Root Cause Analysis

The lack of descriptor pooling stems from several design issues:

### 2.1 No Descriptor Allocator
Descriptors may be allocated directly from D3D12 descriptor heaps without an intermediate allocation layer that can track free slots and implement pooling.

**Current Pattern (Problematic)**:
```cpp
// Allocate descriptor directly, never freed
D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
handle.ptr += index * descriptorSize;  // Linear allocation, no reuse
```

### 2.2 No Free List Management
When resources are destroyed, their descriptors may not be returned to a free list for reuse, causing permanent leakage of descriptor slots.

### 2.3 Large Heap Over-Allocation
Without pooling, the code may create very large descriptor heaps "just in case", wasting memory when only a small fraction is used.

### 2.4 No Descriptor Handle Caching
Same descriptor configurations (e.g., SRV for a texture) may be recreated multiple times instead of being cached and reused.

### 2.5 CPU vs GPU Heap Confusion
CPU descriptor heaps (for non-shader-visible descriptors like RTV/DSV) and GPU descriptor heaps (shader-visible for SRV/UAV/CBV/Sampler) may not be managed separately, causing inefficiencies.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Descriptor Heaps Overview**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps
   - Comprehensive guide to descriptor heap types and usage

2. **Descriptor Handles**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-handles
   - CPU vs GPU descriptor handles

3. **Creating Descriptors**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptors
   - Methods for creating SRV, UAV, RTV, DSV, samplers

4. **Descriptor Heap Best Practices**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview
   - Efficiency guidelines and pooling strategies

5. **Resource Binding**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/resource-binding
   - How descriptors are used in root signatures

**Sample Code**:

6. **MiniEngine Descriptor Allocator**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DescriptorHeap.cpp
   - Reference implementation of descriptor pooling

7. **D3D12 Hello World - Descriptor Management**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld
   - Basic descriptor heap usage patterns

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for descriptor heap creation**:
```
Pattern: "CreateDescriptorHeap" OR "ID3D12DescriptorHeap"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/DescriptorHeap*.cpp
Focus: Where and how descriptor heaps are created
```

**Search for descriptor allocation**:
```
Pattern: "GetCPUDescriptorHandleForHeapStart" OR "allocate" + "descriptor"
Files: src/igl/d3d12/**/*.cpp
Focus: Allocation logic for descriptors
```

**Search for view creation**:
```
Pattern: "CreateShaderResourceView" OR "CreateRenderTargetView" OR "CreateDepthStencilView"
Files: src/igl/d3d12/Texture.cpp, src/igl/d3d12/Framebuffer.cpp
Focus: Where descriptors are created for resources
```

**Search for descriptor handle storage**:
```
Pattern: "D3D12_CPU_DESCRIPTOR_HANDLE" OR "D3D12_GPU_DESCRIPTOR_HANDLE"
Files: src/igl/d3d12/**/*.h
Focus: How descriptor handles are stored in resources
```

### 4.2 Likely File Locations

Based on typical D3D12 backend architecture:
- `src/igl/d3d12/Device.cpp` - Descriptor heap creation
- `src/igl/d3d12/DescriptorHeap.cpp` or `src/igl/d3d12/DescriptorAllocator.cpp` - Allocation logic (if exists)
- `src/igl/d3d12/Texture.cpp` - SRV/UAV descriptor creation
- `src/igl/d3d12/Framebuffer.cpp` - RTV/DSV descriptor creation
- `src/igl/d3d12/Sampler.cpp` - Sampler descriptor creation
- `src/igl/d3d12/Buffer.cpp` - CBV/SRV descriptor creation

### 4.3 Key Data Structures to Find

Look for:
- Descriptor heap objects (`ComPtr<ID3D12DescriptorHeap>`)
- Descriptor size tracking (`UINT descriptorIncrementSize`)
- Allocation indices or offsets
- Free list structures (if any)
- Descriptor handle caches (if any)

---

## 5. Detection Strategy

### 5.1 Add Instrumentation Code

**Step 1: Track descriptor allocations**

Add global statistics:

```cpp
// Descriptor allocation statistics
struct DescriptorStats {
    std::atomic<uint64_t> allocations{0};
    std::atomic<uint64_t> frees{0};
    std::atomic<uint64_t> activeDescriptors{0};
    std::atomic<uint64_t> heapsCreated{0};
    std::atomic<uint64_t> peakDescriptors{0};

    void recordAllocation() {
        allocations++;
        uint64_t active = ++activeDescriptors;
        uint64_t peak = peakDescriptors.load();
        while (active > peak && !peakDescriptors.compare_exchange_weak(peak, active)) {}
    }

    void recordFree() {
        frees++;
        activeDescriptors--;
    }

    void logStats() {
        IGL_LOG_INFO("Descriptor Statistics:\n");
        IGL_LOG_INFO("  Total Allocations: %llu\n", allocations.load());
        IGL_LOG_INFO("  Total Frees: %llu\n", frees.load());
        IGL_LOG_INFO("  Active Descriptors: %llu\n", activeDescriptors.load());
        IGL_LOG_INFO("  Peak Descriptors: %llu\n", peakDescriptors.load());
        IGL_LOG_INFO("  Heaps Created: %llu\n", heapsCreated.load());
        IGL_LOG_INFO("  Leaked Descriptors: %llu\n", allocations.load() - frees.load());
    }
};

static DescriptorStats g_descriptorStats;
```

**Step 2: Instrument allocation points**

Wrap all descriptor allocations:

```cpp
// In descriptor allocation function
D3D12_CPU_DESCRIPTOR_HANDLE allocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    g_descriptorStats.recordAllocation();

    IGL_LOG_DEBUG("Allocating descriptor of type %d, active count: %llu\n",
                  type, g_descriptorStats.activeDescriptors.load());

    // ... actual allocation ...
    return handle;
}

// In descriptor free function (if exists)
void freeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    g_descriptorStats.recordFree();

    IGL_LOG_DEBUG("Freeing descriptor, active count: %llu\n",
                  g_descriptorStats.activeDescriptors.load());

    // ... actual free ...
}
```

**Step 3: Track heap creation**

```cpp
// In CreateDescriptorHeap calls
HRESULT createDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC& desc) {
    g_descriptorStats.heapsCreated++;

    IGL_LOG_INFO("Creating descriptor heap: Type=%d, NumDescriptors=%u, Heaps=%llu\n",
                 desc.Type, desc.NumDescriptors, g_descriptorStats.heapsCreated.load());

    return device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
}
```

### 5.2 Manual Testing Procedure

**Test 1: Run tests and monitor descriptor usage**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

Look for:
- Growing active descriptor count (memory leak)
- Many heap creations
- Allocations without corresponding frees

**Test 2: Stress test with many textures**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

Monitor peak descriptor count.

**Test 3: Run sample applications**
```bash
./build/Debug/samples/TQSession
./build/Debug/samples/MRTSession
```

Check for descriptor leaks over time.

### 5.3 Expected Baseline Metrics

**Problematic Indicators**:
- Allocations >> Frees (memory leak)
- Many heaps created (>5-10 of same type)
- Active descriptors growing unbounded
- No descriptor reuse

**Optimized Targets**:
- Allocations ≈ Frees (small difference for persistent resources)
- Minimal heaps (1-2 per type)
- Active descriptors stabilize after warm-up
- High reuse rate

---

## 6. Fix Guidance

### 6.1 Implement Descriptor Allocator

Create a pooled descriptor allocator:

```cpp
// DescriptorAllocator.h
class DescriptorAllocator {
public:
    DescriptorAllocator(ID3D12Device* device,
                       D3D12_DESCRIPTOR_HEAP_TYPE type,
                       bool shaderVisible = false)
        : device_(device), type_(type), shaderVisible_(shaderVisible) {
        descriptorSize_ = device_->GetDescriptorHandleIncrementSize(type);
    }

    // Allocate a descriptor handle
    struct DescriptorHandle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu;  // Only valid if shader visible
        uint32_t index;
        bool isValid() const { return cpu.ptr != 0; }
    };

    DescriptorHandle allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Try to reuse from free list first
        if (!freeList_.empty()) {
            uint32_t index = freeList_.back();
            freeList_.pop_back();
            return getHandle(index);
        }

        // Allocate new slot
        if (currentHeap_ == nullptr || allocatedInCurrentHeap_ >= heapSize_) {
            createNewHeap();
        }

        uint32_t index = currentHeapBaseIndex_ + allocatedInCurrentHeap_;
        allocatedInCurrentHeap_++;

        return getHandle(index);
    }

    // Free a descriptor for reuse
    void free(const DescriptorHandle& handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        freeList_.push_back(handle.index);
    }

    // Get the heap (for GPU-visible heaps)
    ID3D12DescriptorHeap* getHeap() const { return currentHeap_.Get(); }

private:
    void createNewHeap() {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type_;
        desc.NumDescriptors = heapSize_;
        desc.Flags = shaderVisible_ ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                    : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ComPtr<ID3D12DescriptorHeap> newHeap;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&newHeap));
        if (FAILED(hr)) {
            IGL_LOG_ERROR("Failed to create descriptor heap\n");
            return;
        }

        heaps_.push_back(newHeap);
        currentHeap_ = newHeap;
        currentHeapBaseIndex_ = totalDescriptors_;
        allocatedInCurrentHeap_ = 0;
        totalDescriptors_ += heapSize_;

        IGL_LOG_INFO("Created descriptor heap %zu: Type=%d, Size=%u, ShaderVisible=%d\n",
                     heaps_.size(), type_, heapSize_, shaderVisible_);
    }

    DescriptorHandle getHandle(uint32_t index) const {
        DescriptorHandle handle;
        handle.index = index;

        // Find which heap this index belongs to
        uint32_t heapIndex = index / heapSize_;
        uint32_t offsetInHeap = index % heapSize_;

        auto heap = heaps_[heapIndex];
        handle.cpu = heap->GetCPUDescriptorHandleForHeapStart();
        handle.cpu.ptr += offsetInHeap * descriptorSize_;

        if (shaderVisible_) {
            handle.gpu = heap->GetGPUDescriptorHandleForHeapStart();
            handle.gpu.ptr += offsetInHeap * descriptorSize_;
        } else {
            handle.gpu.ptr = 0;
        }

        return handle;
    }

    ID3D12Device* device_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    bool shaderVisible_;
    UINT descriptorSize_;

    static constexpr UINT heapSize_ = 1024;  // Descriptors per heap

    std::vector<ComPtr<ID3D12DescriptorHeap>> heaps_;
    ComPtr<ID3D12DescriptorHeap> currentHeap_;
    uint32_t currentHeapBaseIndex_ = 0;
    uint32_t allocatedInCurrentHeap_ = 0;
    uint32_t totalDescriptors_ = 0;

    std::vector<uint32_t> freeList_;
    std::mutex mutex_;
};
```

### 6.2 Create Allocators for Each Heap Type

In Device class:

```cpp
// In Device.h
class Device {
private:
    // CPU descriptor allocators (non-shader-visible)
    std::unique_ptr<DescriptorAllocator> rtvAllocator_;
    std::unique_ptr<DescriptorAllocator> dsvAllocator_;

    // GPU descriptor allocators (shader-visible)
    std::unique_ptr<DescriptorAllocator> srvAllocator_;  // SRV/UAV/CBV
    std::unique_ptr<DescriptorAllocator> samplerAllocator_;

public:
    DescriptorAllocator& getRTVAllocator() { return *rtvAllocator_; }
    DescriptorAllocator& getDSVAllocator() { return *dsvAllocator_; }
    DescriptorAllocator& getSRVAllocator() { return *srvAllocator_; }
    DescriptorAllocator& getSamplerAllocator() { return *samplerAllocator_; }
};

// In Device initialization
Result Device::initialize() {
    // ... device creation ...

    // Create descriptor allocators
    rtvAllocator_ = std::make_unique<DescriptorAllocator>(
        device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    dsvAllocator_ = std::make_unique<DescriptorAllocator>(
        device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

    srvAllocator_ = std::make_unique<DescriptorAllocator>(
        device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    samplerAllocator_ = std::make_unique<DescriptorAllocator>(
        device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true);

    IGL_LOG_INFO("Descriptor allocators initialized\n");

    return Result();
}
```

### 6.3 Use Allocators in Resource Creation

**Example: Texture SRV creation**

```cpp
// In Texture.cpp
class Texture {
private:
    DescriptorAllocator::DescriptorHandle srvHandle_;

public:
    Result createShaderResourceView() {
        auto& allocator = device_->getSRVAllocator();

        // Allocate descriptor from pool
        srvHandle_ = allocator.allocate();
        if (!srvHandle_.isValid()) {
            return Result(Result::Code::RuntimeError, "Failed to allocate SRV descriptor");
        }

        // Create the SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        // ... fill in SRV desc ...

        device_->getDevice()->CreateShaderResourceView(
            resource_.Get(), &srvDesc, srvHandle_.cpu);

        IGL_LOG_DEBUG("Created SRV at descriptor index %u\n", srvHandle_.index);

        return Result();
    }

    // Destructor must free descriptor
    ~Texture() {
        if (srvHandle_.isValid()) {
            device_->getSRVAllocator().free(srvHandle_);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getSRVHandle() const { return srvHandle_.cpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUSRVHandle() const { return srvHandle_.gpu; }
};
```

**Example: Render target view creation**

```cpp
// In Framebuffer or Texture (for render targets)
class Framebuffer {
private:
    std::vector<DescriptorAllocator::DescriptorHandle> rtvHandles_;

public:
    Result createRenderTargetViews(const FramebufferDesc& desc) {
        auto& allocator = device_->getRTVAllocator();

        rtvHandles_.reserve(desc.colorAttachments.size());

        for (const auto& attachment : desc.colorAttachments) {
            // Allocate RTV descriptor
            auto handle = allocator.allocate();
            if (!handle.isValid()) {
                return Result(Result::Code::RuntimeError, "Failed to allocate RTV");
            }

            // Create RTV
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            // ... fill in RTV desc ...

            device_->getDevice()->CreateRenderTargetView(
                attachment.texture->getResource(), &rtvDesc, handle.cpu);

            rtvHandles_.push_back(handle);
        }

        return Result();
    }

    ~Framebuffer() {
        auto& allocator = device_->getRTVAllocator();
        for (auto& handle : rtvHandles_) {
            allocator.free(handle);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getRTVHandle(uint32_t index) const {
        return rtvHandles_[index].cpu;
    }
};
```

### 6.4 Implement Descriptor Caching (Optional Enhancement)

For frequently used descriptor configurations:

```cpp
// Cache descriptors by resource + view configuration
class DescriptorCache {
public:
    struct Key {
        ID3D12Resource* resource;
        uint64_t viewDescHash;  // Hash of view desc

        bool operator==(const Key& other) const {
            return resource == other.resource && viewDescHash == other.viewDescHash;
        }
    };

    struct KeyHasher {
        size_t operator()(const Key& key) const {
            return std::hash<void*>()(key.resource) ^ key.viewDescHash;
        }
    };

    DescriptorAllocator::DescriptorHandle getOrCreate(
        const Key& key,
        std::function<DescriptorAllocator::DescriptorHandle()> creator) {

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            cacheHits_++;
            return it->second;
        }

        cacheMisses_++;
        auto handle = creator();
        cache_[key] = handle;
        return handle;
    }

private:
    std::unordered_map<Key, DescriptorAllocator::DescriptorHandle, KeyHasher> cache_;
    std::mutex mutex_;
    uint64_t cacheHits_ = 0;
    uint64_t cacheMisses_ = 0;
};
```

### 6.5 Handle Shutdown Properly

Ensure allocators are destroyed after all resources:

```cpp
// In Device destructor or shutdown
Device::~Device() {
    // Wait for GPU to finish
    waitForIdle();

    // Resources should be destroyed first (will free descriptors)
    // Then destroy allocators

    srvAllocator_.reset();
    samplerAllocator_.reset();
    rtvAllocator_.reset();
    dsvAllocator_.reset();

    IGL_LOG_INFO("Descriptor allocators destroyed\n");
}
```

---

## 7. Testing Requirements

### 7.1 Functional Testing

**Test 1: Verify basic functionality**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: All existing tests pass, no visual regressions.

**Test 2: Texture tests (SRV allocation)**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Expected**: Textures render correctly, descriptors allocated and freed.

**Test 3: Framebuffer tests (RTV/DSV allocation)**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Framebuffer*"
```

**Expected**: Render targets work correctly with pooled descriptors.

**Test 4: Run sample applications**
```bash
./build/Debug/samples/Textured3DCube
./build/Debug/samples/TQSession
./build/Debug/samples/MRTSession
```

**Expected**: All render correctly, no descriptor leaks.

### 7.2 Memory Testing

**Test 1: Check for descriptor leaks**

Run tests and examine statistics:
```
Descriptor Statistics:
  Total Allocations: 1523
  Total Frees: 1520
  Leaked Descriptors: 3  (acceptable for persistent resources)
```

**Test 2: Stress test descriptor allocation/free**

Create test that allocates and frees many resources:
```cpp
TEST(DescriptorAllocator, StressTest) {
    for (int i = 0; i < 10000; ++i) {
        auto texture = device->createTexture(desc);
        // Use texture
        texture.reset();  // Should free descriptor
    }
    // Check no descriptor leak
    EXPECT_EQ(allocator.getActiveCount(), 0);
}
```

**Test 3: Verify descriptor reuse**

Check logs for free list usage:
```
Allocating descriptor from free list (reuse)
```

### 7.3 Performance Testing

**Metrics to Collect**:
- Number of heaps created (should be minimal)
- Active descriptor count (should stabilize)
- Free list utilization
- Allocation/free overhead

**Target Metrics**:
- ≤2 heaps per type (1 initial + 1 growth at most for complex scenes)
- Active descriptors plateau after warm-up
- >80% descriptor reuse rate
- Negligible allocation overhead

### 7.4 Test Modification Restrictions

**CRITICAL CONSTRAINTS**:
- Do NOT modify any existing test assertions
- Do NOT change test expected values
- Do NOT alter test logic or flow
- Changes must be implementation-only, not test-visible
- All tests must pass without modification

If tests fail, fix the allocator implementation, not the tests.

---

## 8. Definition of Done

### 8.1 Implementation Checklist

- [ ] DescriptorAllocator class implemented with free list
- [ ] Separate allocators created for RTV, DSV, SRV, Sampler
- [ ] Texture class uses allocator for SRV/UAV
- [ ] Framebuffer class uses allocator for RTV/DSV
- [ ] Sampler class uses allocator for sampler descriptors
- [ ] Buffer class uses allocator for CBV/SRV
- [ ] All resources properly free descriptors on destruction
- [ ] Instrumentation added for tracking allocations/frees
- [ ] Statistics logging implemented

### 8.2 Testing Checklist

- [ ] All unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"`
- [ ] No visual regressions in sample applications
- [ ] Descriptor statistics show proper allocation/free balance
- [ ] No descriptor leaks detected
- [ ] Stress tests pass without running out of descriptors
- [ ] Descriptor reuse confirmed via logging

### 8.3 Performance Checklist

- [ ] Heap count minimized (1-2 per type)
- [ ] Active descriptor count stabilizes
- [ ] Free list shows reuse activity
- [ ] No performance regression

### 8.4 Documentation Checklist

- [ ] Allocator usage patterns documented
- [ ] Heap sizing strategy documented
- [ ] Descriptor lifecycle documented

### 8.5 Sign-Off Criteria

**Before proceeding with this task, YOU MUST**:
1. Read and understand all 11 sections of this document
2. Understand D3D12 descriptor heap types and usage
3. Verify you can locate descriptor allocation code
4. Confirm you can build and run the test suite
5. Get explicit approval to proceed with implementation

**Do not make any code changes until all criteria are met and approval is given.**

---

## 9. Related Issues

### 9.1 Blocking Issues
None - this can be implemented independently.

### 9.2 Blocked Issues
- Improved descriptor management enables more complex scenes
- May reveal other resource management issues

### 9.3 Related Issues
- Memory management improvements complement this optimization
- Enables future enhancements like bindless rendering

---

## 10. Implementation Priority

**Priority Level**: MEDIUM

**Rationale**:
- Improves memory efficiency and prevents descriptor exhaustion
- Reduces CPU overhead in descriptor management
- Relatively isolated change
- Low risk if implemented carefully
- Standard D3D12 best practice

**Recommended Order**:
1. Add instrumentation to measure baseline (Day 1)
2. Implement DescriptorAllocator class (Day 1-2)
3. Integrate with Texture/Framebuffer/Sampler classes (Day 2-3)
4. Testing and validation (Day 3-4)
5. Performance analysis and refinement (Day 4)

**Estimated Timeline**:
- Day 1: Investigation, instrumentation, allocator design
- Day 2: DescriptorAllocator implementation
- Day 3: Integration with resource classes
- Day 4: Testing, validation, and final refinement

---

## 11. References

### 11.1 Microsoft Official Documentation
- Descriptor Heaps: https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps
- Descriptor Handles: https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-handles
- Creating Descriptors: https://docs.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptors
- Descriptor Heap Best Practices: https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview
- Resource Binding: https://docs.microsoft.com/en-us/windows/win32/direct3d12/resource-binding

### 11.2 Sample Code References
- MiniEngine DescriptorHeap: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DescriptorHeap.cpp
  - Excellent reference implementation
- D3D12 Hello World: https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld

### 11.3 Additional Reading
- Descriptor Management Strategies: https://therealmjp.github.io/posts/bindless-texturing-for-deferred-rendering-and-decals/
- D3D12 Memory Management: https://docs.microsoft.com/en-us/windows/win32/direct3d12/memory-management-strategies

### 11.4 Internal Codebase
- Search for "CreateDescriptorHeap" in src/igl/d3d12/
- Review current descriptor allocation in Texture.cpp, Framebuffer.cpp
- Check for existing allocator infrastructure

---

**Document Version**: 1.0
**Last Updated**: 2025-11-12
**Author**: Development Team
**Reviewer**: [Pending]
