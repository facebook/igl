# DX12-NEW-03: Synchronous GPU Waits in Upload Path

**Severity:** High
**Category:** Command Queues & Synchronization
**Status:** Open
**Related Issues:** G-002 (Excessive waitForGPU calls), B-004 (Synchronous GPU wait in map())

---

## Problem Statement

Buffer and texture upload operations call `ctx_->waitForGPU()` immediately after executing copy commands, blocking the CPU until **all GPU work completes**. This defeats the entire purpose of asynchronous upload queues and negates the upload ring buffer optimization.

**The Impact:**
- CPU stalls for 1-3ms per resource creation
- Applications creating 50+ resources at startup: 50-150ms blocked time
- Rendering and upload cannot overlap
- Frame rate drops during asset streaming
- Contradicts Microsoft's explicit guidance to use fences instead of device-wide waits

---

## Root Cause Analysis

### Current Implementation (`Device.cpp:360-485`):

```cpp
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                               Result* outResult) const {
  // Create GPU buffer
  auto buffer = std::make_shared<Buffer>(*ctx_, desc, outResult);

  // If initial data provided, upload it
  if (desc.data != nullptr) {
    // Get upload command list
    auto uploadCmdList = getUploadCommandList();

    // Copy data to staging buffer and record copy commands
    // ... staging buffer creation ...
    uploadCmdList->CopyBufferRegion(buffer->getResource(), 0,
                                    stagingBuffer.Get(), 0, desc.length);

    // Execute upload commands
    ID3D12CommandList* lists[] = { uploadCmdList.Get() };
    ctx_->getUploadQueue()->ExecuteCommandLists(1, lists);

    // BUG: Synchronous wait blocks CPU until GPU finishes!
    ctx_->waitForGPU();  // ← BLOCKS 1-3ms per buffer!

    // Staging buffer destroyed after wait
    // (but we should track it with fence instead)
  }

  return buffer;
}
```

### Why This Is Wrong:

**Microsoft's Guidance:**
> "Don't use device-wide waits like `Flush()`. Instead, signal a fence after command list execution and wait on that specific fence value." - [Uploading Resources](https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources#synchronize-using-fences)

**Correct Pattern:**
```cpp
// Execute upload
uploadQueue->ExecuteCommandLists(1, lists);

// Signal fence
uploadQueue->Signal(uploadFence, ++fenceValue);

// Track staging buffer with fence
trackUploadBuffer(stagingBuffer, fenceValue);

// Return immediately (CPU doesn't wait!)
// GPU processes upload asynchronously
// Staging buffer retired when fence completes
```

### Performance Impact Example:

**Scenario:** Load 100 textures at level start

**Current (Broken):**
```
Frame 0: Create texture 1 → Upload → waitForGPU (2ms) → CPU blocked
Frame 0: Create texture 2 → Upload → waitForGPU (2ms) → CPU blocked
...
Frame 0: Create texture 100 → Upload → waitForGPU (2ms) → CPU blocked
Total: 200ms of CPU waiting for GPU!
```

**Correct (Async):**
```
Frame 0: Create texture 1 → Upload → Signal fence 1 → Continue immediately
Frame 0: Create texture 2 → Upload → Signal fence 2 → Continue immediately
...
Frame 0: Create texture 100 → Upload → Signal fence 100 → Continue immediately
Total: ~5ms (no waiting, GPU processes uploads in parallel)
```

---

## Official Documentation References

1. **Fence-Based Synchronization**:
   - [Synchronize using Fences](https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources#synchronize-using-fences)
   - Quote: "Use fences to synchronize upload work. Signal after ExecuteCommandLists, then check fence completion before reusing resources."

2. **Avoid Device Waits**:
   - [Multi-Engine Synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
   - Quote: "Device-wide waits (like Flush or waiting on all queues) should be avoided in production code."

3. **MiniEngine Upload Pattern**:
   - [DynamicUploadHeap.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicUploadHeap.cpp)
   - Shows async upload with fence tracking, no `waitForGPU` calls

4. **DirectX 12 Best Practices**:
   - [Performance Tips](https://learn.microsoft.com/en-us/windows/win32/direct3d12/performance-tips)
   - Lists "Minimize CPU waits on GPU completion" as a top optimization

---

## Code Location Strategy

### Files to Modify:

1. **Device.cpp** (`createBuffer` with initial data):
   - Search for: `ctx_->waitForGPU()` after `ExecuteCommandLists`
   - Context: Buffer creation with upload
   - Action: Replace with fence signal + tracking

2. **Device.cpp** (`createTexture` with initial data):
   - Search for: Similar `waitForGPU()` pattern in texture upload
   - Context: Texture creation with upload
   - Action: Same async pattern

3. **Buffer.cpp** (`upload` method):
   - Search for: `ctx_->waitForGPU()` calls
   - Context: Runtime buffer uploads
   - Action: Replace with async fence tracking

4. **Texture.cpp** (`upload` / `uploadInternal` methods):
   - Search for: `ctx_->waitForGPU()` in upload paths
   - Context: Runtime texture uploads
   - Action: Replace with async fence tracking

5. **Device.cpp** (`getUploadCommandAllocator` method):
   - Search for: Allocator pool management
   - Context: Should retire allocators based on fence, not immediate wait
   - Action: Track allocators with fence values for deferred retirement

---

## Detection Strategy

### How to Reproduce Performance Issue:

1. **Startup Load Test**:
   ```cpp
   auto startTime = std::chrono::high_resolution_clock::now();

   // Create 100 buffers with initial data
   std::vector<std::shared_ptr<IBuffer>> buffers;
   for (int i = 0; i < 100; i++) {
     BufferDesc desc;
     desc.length = 1024 * 1024;  // 1MB each
     desc.usage = BufferUsageBits::Vertex;
     std::vector<uint8_t> data(desc.length, 0xFF);
     desc.data = data.data();

     buffers.push_back(device->createBuffer(desc, nullptr));
   }

   auto endTime = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
       endTime - startTime).count();

   // Current (BROKEN): ~200-300ms (2-3ms per buffer × 100)
   // Fixed (ASYNC): ~10-20ms (no CPU waits)
   printf("Buffer creation time: %lld ms\n", duration);
   ```

2. **Profile with PIX**:
   - CPU timeline shows long gaps labeled "WaitForGPU"
   - GPU timeline shows upload queue idle while CPU waits
   - No overlap between rendering and uploading

3. **Frame Rate Drop Test**:
   ```cpp
   // Stream textures during gameplay
   while (rendering) {
     if (needsNewTexture) {
       createTextureWithData();  // Causes 2ms hitch
       // Frame rate drops from 60fps to 30fps during streaming
     }
     renderFrame();
   }
   ```

### Verification After Fix:

1. Buffer creation test drops from 200ms → 20ms
2. PIX shows upload queue busy while CPU continues
3. No frame rate drops during texture streaming
4. Rendering and uploading overlap in timeline

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Create Async Upload Helper

**File:** `src/igl/d3d12/Device.h`

**Add to Device class:**
```cpp
private:
  // Upload fence for async uploads
  Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence_;
  std::atomic<uint64_t> uploadFenceValue_{0};

  // Track upload command allocators with fence values
  struct PendingAllocator {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    uint64_t fenceValue;
  };
  std::vector<PendingAllocator> pendingAllocators_;
  std::mutex allocatorMutex_;

public:
  // Signal upload fence and return value
  uint64_t signalUploadFence();

  // Retire completed allocators
  void retireUploadAllocators();
```

**Implementation in Device.cpp:**
```cpp
uint64_t Device::signalUploadFence() {
  uint64_t fenceValue = ++uploadFenceValue_;
  ctx_->getUploadQueue()->Signal(uploadFence_.Get(), fenceValue);
  return fenceValue;
}

void Device::retireUploadAllocators() {
  std::lock_guard<std::mutex> lock(allocatorMutex_);

  uint64_t completedValue = uploadFence_->GetCompletedValue();

  auto it = pendingAllocators_.begin();
  while (it != pendingAllocators_.end()) {
    if (it->fenceValue <= completedValue) {
      // Allocator is safe to reuse
      it->allocator->Reset();
      uploadAllocatorPool_.push_back(std::move(it->allocator));
      it = pendingAllocators_.erase(it);
    } else {
      ++it;
    }
  }
}
```

---

#### Step 2: Fix Buffer Creation with Initial Data

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createBuffer` method where `waitForGPU()` is called

**Current (BROKEN):**
```cpp
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                               Result* outResult) const {
  auto buffer = std::make_shared<Buffer>(*ctx_, desc, outResult);

  if (desc.data != nullptr) {
    auto uploadCmdList = getUploadCommandList();
    auto stagingBuffer = createStagingBuffer(desc.length);

    // Copy to staging and record commands
    // ...

    // Execute
    ID3D12CommandList* lists[] = { uploadCmdList.Get() };
    ctx_->getUploadQueue()->ExecuteCommandLists(1, lists);

    // BUG: Blocks CPU!
    ctx_->waitForGPU();
  }

  return buffer;
}
```

**Fixed (ASYNC):**
```cpp
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                               Result* outResult) const {
  auto buffer = std::make_shared<Buffer>(*ctx_, desc, outResult);

  if (desc.data != nullptr) {
    auto uploadCmdList = getUploadCommandList();
    auto uploadAllocator = getCurrentUploadAllocator();
    auto stagingBuffer = createStagingBuffer(desc.length);

    // Copy to staging and record commands
    // ...

    // Execute upload
    ID3D12CommandList* lists[] = { uploadCmdList.Get() };
    ctx_->getUploadQueue()->ExecuteCommandLists(1, lists);

    // FIXED: Signal fence and track asynchronously
    uint64_t fenceValue = signalUploadFence();

    // Track staging buffer for deferred cleanup
    trackUploadBuffer(stagingBuffer, fenceValue);

    // Track allocator for deferred reuse
    {
      std::lock_guard<std::mutex> lock(allocatorMutex_);
      pendingAllocators_.push_back({uploadAllocator, fenceValue});
    }

    // Return immediately - no CPU wait!
  }

  return buffer;
}
```

---

#### Step 3: Fix Texture Creation with Initial Data

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createTexture` method with upload logic

**Apply:** Same pattern as Step 2:
```cpp
// After ExecuteCommandLists:
uint64_t fenceValue = signalUploadFence();
trackUploadBuffer(stagingBuffer, fenceValue);
trackUploadAllocator(uploadAllocator, fenceValue);
// NO waitForGPU!
```

---

#### Step 4: Fix Runtime Buffer Upload

**File:** `src/igl/d3d12/Buffer.cpp`

**Locate:** `upload` method

**Current:**
```cpp
Result Buffer::upload(const void* data, const BufferRangeDesc& range) {
  // ... upload logic ...
  ctx_->getUploadQueue()->ExecuteCommandLists(1, lists);
  ctx_->waitForGPU();  // BUG
  return Result();
}
```

**Fixed:**
```cpp
Result Buffer::upload(const void* data, const BufferRangeDesc& range) {
  // ... upload logic ...
  ctx_->getUploadQueue()->ExecuteCommandLists(1, lists);

  // FIXED: Async with fence
  uint64_t fenceValue = device_->signalUploadFence();
  device_->trackUploadBuffer(stagingBuffer, fenceValue);

  return Result();
}
```

---

#### Step 5: Fix Runtime Texture Upload

**File:** `src/igl/d3d12/Texture.cpp`

**Locate:** `upload` and `uploadInternal` methods

**Apply:** Same async pattern as Step 4

---

#### Step 6: Retire Resources in Frame Loop

**File:** `src/igl/d3d12/CommandQueue.cpp` (or wherever frame submission occurs)

**In frame submission or `submit` method:**
```cpp
void CommandQueue::submit(const CommandBuffer& commandBuffer) {
  // ... existing submission logic ...

  // Process completed uploads (non-blocking)
  device_.retireUploadAllocators();
  device_.processCompletedUploads();  // From DX12-NEW-02

  // ... rest of frame logic ...
}
```

**Rationale:** Check for completed uploads once per frame, not per resource.

---

#### Step 7: Handle Immediate-Read Cases (Map After Upload)

**Special Case:** If code needs to read buffer immediately after upload:

**File:** `src/igl/d3d12/Buffer.cpp`

**In `map` method:**
```cpp
void* Buffer::map(const BufferRangeDesc& range, Result* outResult) {
  // If this buffer has pending uploads, wait for them
  if (hasPendingUpload()) {
    // Wait only for THIS buffer's upload fence
    uint64_t uploadFenceValue = getPendingUploadFenceValue();
    device_->waitForUploadFence(uploadFenceValue);
  }

  // Now safe to map
  // ...
}
```

**Rationale:** Wait selectively only when needed, not for every upload.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Specific upload/sync tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Buffer*Upload*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*Upload*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Sync*"
```

**Test Modifications Allowed:**
- ✅ Add async upload performance tests
- ✅ Add fence timing validation tests
- ❌ **DO NOT modify cross-platform test logic**

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass with improved performance
./test_all_sessions.bat
```

**Expected Changes:**
- Sessions start faster (less upload time)
- No frame rate drops during texture streaming
- Smoother gameplay in interactive sessions

**Modifications Allowed:**
- ✅ None required (backend optimization)
- ❌ **DO NOT modify session logic**

### Performance Benchmark (New Test):

```cpp
TEST(D3D12PerformanceTest, AsyncUploadSpeed) {
  auto device = createD3D12Device();

  auto start = std::chrono::high_resolution_clock::now();

  // Create 100 buffers with data
  std::vector<std::shared_ptr<IBuffer>> buffers;
  for (int i = 0; i < 100; i++) {
    BufferDesc desc;
    desc.length = 1024 * 1024;  // 1MB
    std::vector<uint8_t> data(desc.length, i);
    desc.data = data.data();

    buffers.push_back(device->createBuffer(desc, nullptr));
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - start).count();

  // Should be < 50ms (async), not > 200ms (sync)
  EXPECT_LT(duration, 50)
      << "Buffer creation too slow: " << duration << "ms";

  // Wait for all uploads to complete
  device->waitForIdle();

  // Verify buffers are valid (uploads succeeded)
  for (auto& buf : buffers) {
    EXPECT_NE(buf, nullptr);
  }
}
```

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] Performance test shows <50ms for 100 buffer uploads (was >200ms)
- [ ] PIX timeline shows upload/render overlap (no CPU waits)
- [ ] No `waitForGPU()` calls in upload paths
- [ ] Frame rate stable during texture streaming
- [ ] No validation errors or resource corruption

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Performance benchmark passes (<50ms)
2. PIX shows async behavior (overlapping work)
3. All render sessions pass with improved startup time

**Post in chat:**
```
DX12-NEW-03 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Performance test: PASS (upload time: XXms, was 200+ms)
- PIX validation: Async uploads confirmed

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- **DX12-NEW-02** (Upload fence synchronization) - Must fix fence tracking first

### Blocks:
- **G-001** (Barrier batching) - Can't optimize barriers while uploads block
- **G-004** (PSO caching) - Startup time improvements need async uploads

### Related:
- **B-004** (Synchronous GPU wait in map) - Same pattern, different context

---

## Implementation Priority

**Priority:** P0 - HIGH (Performance Blocker)
**Estimated Effort:** 6-8 hours
**Risk:** Medium (touches multiple upload paths, needs careful testing)
**Impact:** VERY HIGH - 10x upload performance improvement

---

## References

- [Uploading Resources - Synchronize using Fences](https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources#synchronize-using-fences)
- [Multi-Engine Synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
- [MiniEngine Async Upload Pattern](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicUploadHeap.cpp)
- [DirectX 12 Performance Tips](https://learn.microsoft.com/en-us/windows/win32/direct3d12/performance-tips)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
