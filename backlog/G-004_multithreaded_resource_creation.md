# G-004: Resource Creation Not Parallelized Across Threads

**Severity:** Low
**Category:** CPU Performance
**Status:** Open
**Related Issues:** D-004 (Command Queue Thread Safety), D-006 (Command Allocator Contention)
**Priority:** P2-Low

---

## Problem Statement

Resource creation (textures, buffers, render targets, etc.) is serialized through a single mutex lock in Device::createResource() and related factory methods. This creates a CPU bottleneck when loading game assets or populating large material libraries, as only one thread can create resources at a time. D3D12 Device methods are thread-safe for resource creation, but IGL serializes them unnecessarily.

**Impact Analysis:**
- **Asset Loading Bottleneck:** Loading large asset packs or material libraries stalls threads waiting for lock
- **Startup Time:** Initial scene loading with hundreds of textures/buffers is CPU-bound on lock acquisition
- **Parallelism Loss:** Multi-threaded asset loading cannot utilize multiple cores effectively
- **Memory Allocation:** Device::createResource() may block other frame-critical operations

**The Limitation:**
- D3D12 requires synchronization for descriptor heap allocation, but resource creation itself is thread-safe
- Current implementation uses single device-level lock for all resource operations
- No per-resource-type locks or lock-free data structures
- Background loading threads are serialized at device factory level

---

## Root Cause Analysis

### Current Implementation (Device.cpp):

```cpp
std::shared_ptr<IBuffer> Device::createBuffer(
    const BufferDesc& desc,
    const void* data) {
  // G-004: Serializing under single device lock!
  std::lock_guard<std::mutex> lock(deviceMutex_);

  // Create D3D12 buffer resource
  ComPtr<ID3D12Resource> d3d12Buffer;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Buffer));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("Device::createBuffer: Failed to create resource (0x%08X)\n", hr);
    return nullptr;
  }

  // ... create wrapper ...
  return std::make_shared<Buffer>(d3d12Buffer);
}

std::shared_ptr<ITexture> Device::createTexture(
    const TextureDesc& desc,
    const void* data) {
  // G-004: Same serialization issue
  std::lock_guard<std::mutex> lock(deviceMutex_);

  // Create texture resource...
}

std::shared_ptr<ISamplerState> Device::createSamplerState(
    const SamplerStateDesc& desc) {
  // G-004: Even samplers serialized under same lock
  std::lock_guard<std::mutex> lock(deviceMutex_);

  // Create sampler...
}
```

### Why This Is Wrong:

1. **Single Lock:** All resource creation serialized under `deviceMutex_`
2. **No Parallelism:** Only one thread can create resources at a time
3. **Long Lock Hold:** Lock held for entire resource creation + descriptor allocation
4. **No Per-Type Locks:** Texture and buffer creation block each other
5. **Blocking Descriptor Allocation:** Most of lock time spent in descriptor heap allocation

---

## Official Documentation References

1. **D3D12 Thread Safety**:
   - https://learn.microsoft.com/windows/win32/direct3d12/multithreading
   - Key guidance: "ID3D12Device::CreateCommittedResource is thread-safe"

2. **D3D12 Multithreading Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/multithreading#synchronization-and-multi-gpu
   - Recommendations: "Minimize lock scope; use lock-free data structures where possible"

3. **Command List Threading**:
   - https://learn.microsoft.com/windows/win32/direct3d12/recording-command-lists-and-bundles#multithreaded-command-list-recording
   - Pattern: Command allocators per thread, device operations thread-safe

4. **Microsoft Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12MultithreadedRendering
   - Demonstrates multi-threaded resource creation patterns

---

## Code Location Strategy

### Files to Modify:

1. **Device.h** (add per-type locks):
   - Search for: Private member variables
   - Context: Current deviceMutex_
   - Action: Add buffer, texture, sampler-specific mutexes

2. **Device.cpp** (createBuffer implementation):
   - Search for: `std::lock_guard<std::mutex> lock(deviceMutex_);`
   - Context: Buffer creation serialization
   - Action: Replace with bufferMutex_, reduce lock scope

3. **Device.cpp** (createTexture implementation):
   - Search for: Texture creation lock
   - Context: Texture creation serialization
   - Action: Use textureMutex_, lock only descriptor allocation

4. **Device.cpp** (createSamplerState implementation):
   - Search for: Sampler creation lock
   - Context: Sampler state serialization
   - Action: Use samplerMutex_

5. **DescriptorHeapManager.h/cpp** (atomic descriptor allocation):
   - Search for: allocateCbvSrvUav() method
   - Context: Descriptor allocation under lock
   - Action: Implement atomic descriptor allocation for parallel access

---

## Detection Strategy

### How to Reproduce:

```cpp
// Multi-threaded asset loading test
std::vector<std::thread> loaderThreads;
std::vector<std::shared_ptr<ITexture>> textures;
std::mutex textureLock;

auto startTime = std::chrono::high_resolution_clock::now();

// Launch 8 loader threads
for (int t = 0; t < 8; ++t) {
  loaderThreads.emplace_back([&]() {
    for (int i = 0; i < 100; ++i) {
      auto texture = device->createTexture(textureDesc, nullptr);
      {
        std::lock_guard<std::mutex> lock(textureLock);
        textures.push_back(texture);
      }
    }
  });
}

// Wait for completion
for (auto& t : loaderThreads) {
  t.join();
}

auto duration = std::chrono::high_resolution_clock::now() - startTime;
auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

// Before fix: ~5000-8000ms (single thread bottleneck)
// After fix: ~1000-1500ms (8x parallelism)
std::cout << "Asset loading time: " << msecs << "ms\n";
```

### Expected Behavior (After Fix):

- Multi-threaded asset loading shows near-linear scaling (8 threads -> ~8x speedup)
- No lock contention in profiler
- Frame time unaffected by background asset loading

### Verification After Fix:

1. Run multi-threaded asset loading benchmark
2. Measure throughput: textures created per second
3. Verify ~8x improvement with 8 threads
4. Profile lock contention in profiler

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Per-Type Locks

**File:** `src/igl/d3d12/Device.h`

**Locate:** Private member variables section

**Current (PROBLEM):**
```cpp
class Device : public IDevice {
 private:
  ComPtr<ID3D12Device> device_;
  std::mutex deviceMutex_;  // Single lock for everything!

  // ... other members ...
};
```

**Fixed (SOLUTION):**
```cpp
class Device : public IDevice {
 private:
  ComPtr<ID3D12Device> device_;

  // G-004: Per-type locks for parallelized resource creation
  // These are separate from deviceMutex_ which protects device state
  mutable std::mutex bufferMutex_;        // Protects buffer creation + descriptor allocation
  mutable std::mutex textureMutex_;       // Protects texture creation + descriptor allocation
  mutable std::mutex samplerMutex_;       // Protects sampler creation + descriptor allocation
  mutable std::mutex renderTargetMutex_;  // Protects render target descriptor allocation
  mutable std::mutex depthStencilMutex_;  // Protects depth/stencil descriptor allocation

  // Legacy lock for other operations (to be eliminated gradually)
  mutable std::mutex deviceMutex_;

  // ... other members ...
};
```

**Rationale:** Separate locks allow buffers, textures, samplers to be created concurrently.

---

#### Step 2: Minimize Lock Scope in Buffer Creation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createBuffer()` method

**Current (PROBLEM):**
```cpp
std::shared_ptr<IBuffer> Device::createBuffer(
    const BufferDesc& desc,
    const void* data) {
  std::lock_guard<std::mutex> lock(deviceMutex_);  // Lock held for entire operation!

  // Create D3D12 buffer resource
  ComPtr<ID3D12Resource> d3d12Buffer;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Buffer));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("Device::createBuffer: Failed to create resource (0x%08X)\n", hr);
    return nullptr;
  }

  // Allocate descriptor
  uint32_t descriptorIndex = descriptorHeapMgr_->allocateCbvSrvUav();

  // Create wrapper object
  auto buffer = std::make_shared<Buffer>(d3d12Buffer, descriptorIndex);

  // ... register in tracking structures (still under lock) ...
  return buffer;
}  // Lock released here
```

**Fixed (SOLUTION - Step A: Split Descriptor Allocation):**
```cpp
std::shared_ptr<IBuffer> Device::createBuffer(
    const BufferDesc& desc,
    const void* data) {
  // G-004: Phase 1 - Resource creation (no lock needed, D3D12 is thread-safe)
  ComPtr<ID3D12Resource> d3d12Buffer;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Buffer));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("Device::createBuffer: Failed to create resource (0x%08X)\n", hr);
    return nullptr;
  }

  // G-004: Phase 2 - Descriptor allocation (requires lock)
  uint32_t descriptorIndex = UINT32_MAX;
  {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    descriptorIndex = descriptorHeapMgr_->allocateCbvSrvUav();
    if (descriptorIndex == UINT32_MAX) {
      IGL_LOG_ERROR("Device::createBuffer: Descriptor allocation failed\n");
      return nullptr;
    }
  }

  // Phase 3 - Wrapper creation (no lock needed)
  auto buffer = std::make_shared<Buffer>(d3d12Buffer, descriptorIndex);

  // G-004: Phase 4 - Registration (minimal lock scope)
  {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    bufferRegistry_.insert({buffer.get(), buffer});
  }

  return buffer;
}
```

**Rationale:** Split into phases:
1. D3D12 resource creation (thread-safe, no lock)
2. Descriptor allocation (locked, short duration)
3. Wrapper object creation (no lock)
4. Registration (locked, very short)

---

#### Step 3: Apply Same Pattern to Texture Creation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createTexture()` method

**Fixed (SOLUTION):**
```cpp
std::shared_ptr<ITexture> Device::createTexture(
    const TextureDesc& desc,
    const void* data) {
  // G-004: Phase 1 - Validation and descriptor count calculation
  uint32_t descriptorCount = calculateRequiredDescriptors(desc);

  // Phase 2 - D3D12 resource creation (thread-safe, no lock)
  ComPtr<ID3D12Resource> d3d12Texture;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(&d3d12Texture));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("Device::createTexture: Failed to create resource (0x%08X)\n", hr);
    return nullptr;
  }

  // Phase 3 - Descriptor allocation (locked, short duration)
  std::vector<uint32_t> descriptorIndices;
  {
    std::lock_guard<std::mutex> lock(textureMutex_);
    for (uint32_t i = 0; i < descriptorCount; ++i) {
      uint32_t idx = descriptorHeapMgr_->allocateCbvSrvUav();
      if (idx == UINT32_MAX) {
        // Rollback: free allocated descriptors
        for (auto allocIdx : descriptorIndices) {
          descriptorHeapMgr_->freeCbvSrvUav(allocIdx);
        }
        IGL_LOG_ERROR("Device::createTexture: Descriptor allocation failed\n");
        return nullptr;
      }
      descriptorIndices.push_back(idx);
    }
  }

  // Phase 4 - Wrapper creation (no lock)
  auto texture = std::make_shared<Texture>(d3d12Texture, descriptorIndices);

  // Phase 5 - Registration (minimal lock)
  {
    std::lock_guard<std::mutex> lock(textureMutex_);
    textureRegistry_.insert({texture.get(), texture});
  }

  return texture;
}
```

**Rationale:** Same multi-phase approach reduces lock contention for textures.

---

#### Step 4: Apply Same Pattern to Sampler Creation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createSamplerState()` method

**Fixed (SOLUTION):**
```cpp
std::shared_ptr<ISamplerState> Device::createSamplerState(
    const SamplerStateDesc& desc) {
  // G-004: Phase 1 - Create D3D12 sampler (thread-safe, no lock)
  ComPtr<ID3D12CommandList> samplerList;  // Samplers created via descriptor
  // ... sampler descriptor setup ...

  // Phase 2 - Allocate descriptor from sampler heap (requires lock)
  uint32_t samplerIndex = UINT32_MAX;
  {
    std::lock_guard<std::mutex> lock(samplerMutex_);
    samplerIndex = descriptorHeapMgr_->allocateSampler();
    if (samplerIndex == UINT32_MAX) {
      IGL_LOG_ERROR("Device::createSamplerState: Sampler allocation failed\n");
      return nullptr;
    }
  }

  // Phase 3 - Create wrapper (no lock)
  auto sampler = std::make_shared<SamplerState>(samplerIndex, desc);

  // Phase 4 - Register (minimal lock)
  {
    std::lock_guard<std::mutex> lock(samplerMutex_);
    samplerRegistry_.insert({sampler.get(), sampler});
  }

  return sampler;
}
```

**Rationale:** Samplers benefit from separate lock, allowing concurrent buffer/texture creation.

---

#### Step 5: Make Descriptor Allocation More Efficient

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** `allocateCbvSrvUav()` method

**Current (PROBLEM):**
```cpp
uint32_t DescriptorHeapManager::allocateCbvSrvUav() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (freeCbvSrvUav_.empty()) {
    return UINT32_MAX;
  }
  const uint32_t idx = freeCbvSrvUav_.back();
  freeCbvSrvUav_.pop_back();
  return idx;
}
```

**Fixed (SOLUTION - Optional: Atomic Operations):**
```cpp
uint32_t DescriptorHeapManager::allocateCbvSrvUav() {
  // G-004: Try fast path without lock (requires atomic free list)
  // For now, keep existing lock but mark as hot path for future optimization
  std::lock_guard<std::mutex> lock(mutex_);

  if (freeCbvSrvUav_.empty()) {
    IGL_LOG_ERROR("DescriptorHeapManager::allocateCbvSrvUav: Heap exhausted\n");
    return UINT32_MAX;
  }

  const uint32_t idx = freeCbvSrvUav_.back();
  freeCbvSrvUav_.pop_back();

  // G-004: Track allocation for metrics
  ++allocatedCount_;

  return idx;
}
```

**Rationale:** Current implementation is acceptable; future optimization could use lock-free queue.

---

#### Step 6: Update Resource Destruction to Use Per-Type Locks

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `releaseBuffer()` method (if exists)

**Fixed (SOLUTION):**
```cpp
void Device::releaseBuffer(void* bufferPtr) {
  // G-004: Use bufferMutex_ instead of deviceMutex_
  std::lock_guard<std::mutex> lock(bufferMutex_);

  auto it = bufferRegistry_.find(bufferPtr);
  if (it != bufferRegistry_.end()) {
    auto& buffer = it->second;
    // Free descriptor
    descriptorHeapMgr_->freeCbvSrvUav(buffer->getDescriptorIndex());
    // Remove from registry
    bufferRegistry_.erase(it);
  }
}
```

**Rationale:** Consistent use of per-type locks throughout resource lifecycle.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Resource creation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Buffer*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Sampler*"
```

**Test Modifications Allowed:**
- ✅ Add multi-threaded resource creation test
- ✅ Add stress test for concurrent resource creation
- ❌ **DO NOT modify existing test logic**

**New Tests to Add:**
```cpp
// Test concurrent buffer creation
TEST(Device, ConcurrentBufferCreation) {
  auto device = createTestDevice();
  std::vector<std::shared_ptr<IBuffer>> buffers;
  std::vector<std::thread> threads;
  std::mutex resultLock;

  auto startTime = std::chrono::high_resolution_clock::now();

  // Launch 8 threads creating 100 buffers each
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&]() {
      for (int i = 0; i < 100; ++i) {
        auto buffer = device->createBuffer(testBufferDesc, nullptr);
        ASSERT_NE(nullptr, buffer);
        {
          std::lock_guard<std::mutex> lock(resultLock);
          buffers.push_back(buffer);
        }
      }
    });
  }

  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }

  auto duration = std::chrono::high_resolution_clock::now() - startTime;
  auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

  // Verify all buffers created
  EXPECT_EQ(800, buffers.size());

  // Performance assertion (before fix: 5000+ms, after: 1000-1500ms)
  std::cout << "Concurrent creation of 800 buffers: " << msecs << "ms\n";
}

// Test concurrent texture and buffer creation
TEST(Device, ConcurrentMixedResourceCreation) {
  auto device = createTestDevice();
  std::vector<std::shared_ptr<IBuffer>> buffers;
  std::vector<std::shared_ptr<ITexture>> textures;
  std::vector<std::thread> threads;
  std::mutex resultLock;

  // Launch mixed creation threads
  for (int t = 0; t < 4; ++t) {
    // Buffer creation thread
    threads.emplace_back([&]() {
      for (int i = 0; i < 50; ++i) {
        auto buffer = device->createBuffer(testBufferDesc, nullptr);
        ASSERT_NE(nullptr, buffer);
        {
          std::lock_guard<std::mutex> lock(resultLock);
          buffers.push_back(buffer);
        }
      }
    });

    // Texture creation thread
    threads.emplace_back([&]() {
      for (int i = 0; i < 50; ++i) {
        auto texture = device->createTexture(testTextureDesc, nullptr);
        ASSERT_NE(nullptr, texture);
        {
          std::lock_guard<std::mutex> lock(resultLock);
          textures.push_back(texture);
        }
      }
    });
  }

  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }

  // Verify all created
  EXPECT_EQ(200, buffers.size());
  EXPECT_EQ(200, textures.size());
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions must pass (parallelization should be transparent)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required - parallelization is transparent
- ❌ **DO NOT modify session logic**

### Performance Benchmark:

```bash
# Multi-threaded asset loading benchmark
# Expected: 8x speedup with 8 threads (near-linear scaling)
```

---

## Definition of Done

### Completion Criteria:

- [ ] Per-type mutexes added (bufferMutex_, textureMutex_, samplerMutex_, etc.)
- [ ] Lock scope minimized: D3D12 resource creation outside lock
- [ ] Descriptor allocation under separate lock (short-lived)
- [ ] Wrapper object creation outside lock
- [ ] Resource registration under minimal lock
- [ ] All unit tests pass (including new concurrent tests)
- [ ] All render sessions pass without visual changes
- [ ] Performance benchmark shows 6-8x speedup with 8 threads
- [ ] No deadlocks or race conditions in multi-threaded scenarios
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Concurrent resource creation works without errors
2. All resources created successfully with correct properties
3. Performance shows near-linear scaling (6-8x with 8 threads)
4. All render sessions produce identical output

**Post in chat:**
```
G-004 Fix Complete - Ready for Review

Multithreaded Resource Creation:
- Per-Type Locks: bufferMutex_, textureMutex_, samplerMutex_, etc.
- Lock Scope: Minimized to descriptor allocation phase
- Parallelism: D3D12 resource creation happens without lock
- Registration: Brief lock for resource registry updates
- Contention: Separate locks prevent buffer/texture blocking

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Concurrent creation: PASS (800 buffers created concurrently)
- Mixed resources: PASS (buffers and textures in parallel)
- Render sessions: PASS (no visual changes)
- Performance: ~6-8x speedup with 8 threads (near-linear)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **D-004** - Command queue thread safety (related parallelization)
- **D-006** - Command allocator contention (similar per-thread pattern)

---

## Implementation Priority

**Priority:** P2-Low (CPU Performance)
**Estimated Effort:** 3-4 hours
**Risk:** Medium (Lock changes require careful verification to prevent deadlocks)
**Impact:** 6-8x speedup in multi-threaded asset loading scenarios

**Notes:**
- Multi-phase approach separates lock-free operations from locked descriptor allocation
- Per-type locks prevent cross-blocking between resource types
- D3D12 Device methods are inherently thread-safe; only descriptor allocation needs protection
- Future optimization: Replace descriptor allocator mutex with lock-free queue
- Test thoroughly for deadlocks when multiple threads create multiple resource types

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/multithreading
- https://learn.microsoft.com/windows/win32/direct3d12/multithreading#synchronization-and-multi-gpu
- https://learn.microsoft.com/windows/win32/direct3d12/recording-command-lists-and-bundles#multithreaded-command-list-recording
- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12MultithreadedRendering
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommittedresource

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
