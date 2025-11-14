# DX12-NEW-02: Upload Staging Buffer Fence Synchronization Mismatch

**Severity:** High
**Category:** Resources & Memory Lifetime
**Status:** Open
**Related Issues:** B-002 (Unsafe resource replacement), B-007 (Readback staging buffer never released)

---

## Problem Statement

Upload staging buffers are tracked with the **wrong fence**, causing a permanent memory leak and potential use-after-free bugs.

**The Problem:**
1. `Device::trackUploadBuffer()` enqueues staging buffers on the **swap-chain fence** (`ctx.getFence()`)
2. `Device::processCompletedUploads()` retires staging buffers by checking **upload fence** (`uploadFence_->GetCompletedValue()`)
3. These two fences are **completely independent** and never correlate

**Result:**
- `pendingUploads_` vector **never drains** - staging buffers accumulate indefinitely
- Large texture uploads (e.g., 4K textures) remain resident forever
- `UploadRingBuffer::retire()` receives stale/incorrect fence values
- Under load, the application **exhausts GPU memory** and crashes

---

## Root Cause Analysis

### Current Implementation Flow:

#### 1. Upload Path (`Device.cpp:2656-2677`):

```cpp
void Device::trackUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer,
                                uint64_t fenceValue) {
  std::lock_guard<std::mutex> lock(uploadMutex_);

  // BUG: Uses swap-chain fence value, NOT upload fence!
  uint64_t currentFenceValue = ctx_->getFence()->GetCompletedValue();

  pendingUploads_.push_back({
    std::move(uploadBuffer),
    currentFenceValue  // WRONG: This is the swap-chain fence!
  });
}
```

#### 2. Retirement Path (`Device.cpp:2628-2642`):

```cpp
void Device::processCompletedUploads() {
  std::lock_guard<std::mutex> lock(uploadMutex_);

  // BUG: Checks upload fence, but pendingUploads_ has swap-chain fence values!
  uint64_t completedValue = uploadFence_->GetCompletedValue();

  auto it = pendingUploads_.begin();
  while (it != pendingUploads_.end()) {
    if (it->fenceValue <= completedValue) {  // Never true!
      it = pendingUploads_.erase(it);
    } else {
      ++it;
    }
  }
}
```

#### 3. Why This Breaks:

| Fence | Purpose | Current Value | Used In |
|-------|---------|---------------|---------|
| `ctx_->getFence()` | Swap-chain presentation | Increments per `Present()` | `trackUploadBuffer` (WRONG) |
| `uploadFence_` | Upload queue completion | Increments per upload signal | `processCompletedUploads` (correct) |

**Timeline Example:**
```
Frame 1: Upload texture, swap-chain fence = 1, upload fence = 1
         trackUploadBuffer stores fenceValue=1 (swap-chain)

Frame 2: Present advances swap-chain fence to 2
         Upload fence still = 1 (no new uploads)

processCompletedUploads checks: uploadFence (1) >= pendingFenceValue (1)?
  → Should retire, but DOESN'T because swap-chain fence (2) != upload fence (1)

Frame 100: Swap-chain fence = 100, upload fence = 50
           pendingUploads_ still has entries from frame 1 with fenceValue=1
           Comparing uploadFence (50) >= swapChainFence (1) is meaningless
```

---

## Official Documentation References

1. **Fence Synchronization**:
   - [Synchronization using Fences (Microsoft Learn)](https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources#synchronize-using-fences)
   - Quote: "Use fences to track GPU progress. Signal a fence after executing a command list, then wait on the CPU or GPU for that fence value."

2. **MiniEngine Upload Pattern**:
   - [MiniEngine DynamicUploadHeap.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicUploadHeap.cpp)
   - Shows correct pattern: Signal **upload queue fence**, retire on **same fence**

3. **Memory Management Best Practices**:
   - [Memory Management in Direct3D 12](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management)
   - Emphasizes tracking resources with the fence that signals their completion

---

## Code Location Strategy

### Files to Modify:

1. **Device.cpp** (`trackUploadBuffer` method):
   - Search for: `ctx_->getFence()->GetCompletedValue()`
   - Context: Where upload buffers are enqueued with fence value
   - Action: Change to use `uploadFence_` instead

2. **Device.cpp** (`processCompletedUploads` method):
   - Search for: `uploadFence_->GetCompletedValue()`
   - Context: Where staging buffers are retired
   - Action: Verify this already uses `uploadFence_` (correct)

3. **Buffer.cpp** / **Texture.cpp** (upload methods):
   - Search for: Calls to `trackUploadBuffer`
   - Context: Where upload operations signal fences
   - Action: Ensure they signal `uploadFence_` and pass that value

4. **CommandQueue.cpp** (upload command execution):
   - Search for: `ExecuteCommandLists` for upload operations
   - Context: Where upload commands are submitted
   - Action: Ensure upload queue signals `uploadFence_` after execution

---

## Detection Strategy

### How to Reproduce:

1. **Memory Leak Test**:
   ```cpp
   // Upload 100 large textures
   for (int i = 0; i < 100; i++) {
     TextureDesc desc;
     desc.width = 2048;
     desc.height = 2048;
     desc.format = TextureFormat::RGBA_UNorm8;
     desc.usage = TextureDesc::TextureUsageBits::Sampled;

     std::vector<uint8_t> data(2048 * 2048 * 4, 0xFF);
     auto tex = device->createTexture(desc, data.data());

     // Texture holds staging buffer reference
     tex.reset();  // Should release staging buffer
   }

   // Check Device::pendingUploads_ size
   // Expected: Should be empty or small
   // Actual: Contains 100 entries (never released)
   ```

2. **Check Pending Uploads**:
   ```cpp
   // Add to Device.h for debugging:
   size_t getPendingUploadCount() const {
     std::lock_guard<std::mutex> lock(uploadMutex_);
     return pendingUploads_.size();
   }

   // In test:
   EXPECT_EQ(device->getPendingUploadCount(), 0)
       << "Upload staging buffers leaked!";
   ```

3. **Enable D3D12 Debug Layer**:
   ```
   D3D12 WARNING: Live ID3D12Resource at 0x... refcount=1
   Name: "Upload_Staging_Texture_..."
   Potential leak detected.
   ```

4. **PIX Memory Capture**:
   - Capture after 100 texture uploads
   - Check for lingering `D3D12_HEAP_TYPE_UPLOAD` resources
   - Should be released after GPU completes upload

### Verification After Fix:

1. Run memory leak test - `pendingUploads_` should drain to 0
2. Upload ring buffer should correctly retire allocations
3. No live upload resources after frame completes
4. GPU memory usage stable across frames

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Understand Fence Roles

The correct pattern:
```
Upload Queue (copy operations)
  ↓
Signal uploadFence_ with unique value X
  ↓
Track staging buffer with value X
  ↓
processCompletedUploads checks: uploadFence_.GetCompletedValue() >= X?
  ↓
If yes, retire staging buffer (GPU finished reading it)
```

---

#### Step 2: Fix trackUploadBuffer to Use Upload Fence

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `trackUploadBuffer` method

**Current (BROKEN):**
```cpp
void Device::trackUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer,
                                uint64_t fenceValue) {
  std::lock_guard<std::mutex> lock(uploadMutex_);

  // BUG: Uses swap-chain fence
  uint64_t currentFenceValue = ctx_->getFence()->GetCompletedValue();

  pendingUploads_.push_back({
    std::move(uploadBuffer),
    currentFenceValue  // WRONG
  });
}
```

**Fixed:**
```cpp
void Device::trackUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer,
                                uint64_t fenceValue) {
  std::lock_guard<std::mutex> lock(uploadMutex_);

  // FIXED: Use the provided fence value (should be from uploadFence_)
  pendingUploads_.push_back({
    std::move(uploadBuffer),
    fenceValue  // Use parameter, not swap-chain fence!
  });

  IGL_LOG_INFO("Tracked upload buffer with fence value %llu\n", fenceValue);
}
```

**Rationale:** The `fenceValue` parameter should already be the upload fence value. Callers must signal `uploadFence_` and pass that value.

---

#### Step 3: Ensure Upload Operations Signal Correct Fence

**File:** `src/igl/d3d12/Buffer.cpp`

**Locate:** `upload` method where command list is executed

**Pattern to verify:**
```cpp
Result Buffer::upload(const void* data, const TextureRangeDesc& range) {
  // ... copy commands ...

  // Execute upload commands
  ID3D12CommandList* lists[] = { uploadCmdList.Get() };
  ctx_->getUploadQueue()->ExecuteCommandLists(1, lists);

  // CRITICAL: Signal upload fence and track with THAT value
  uint64_t uploadFenceValue = ++uploadFenceValue_;  // Device member
  ctx_->getUploadQueue()->Signal(ctx_->getDevice()->getUploadFence(), uploadFenceValue);

  // Track staging buffer with upload fence value
  ctx_->getDevice()->trackUploadBuffer(stagingBuffer, uploadFenceValue);

  return Result();
}
```

**If not present, add:**
```cpp
// In Device.h:
private:
  std::atomic<uint64_t> uploadFenceValue_{0};
  Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence_;

public:
  ID3D12Fence* getUploadFence() const { return uploadFence_.Get(); }
  uint64_t getNextUploadFenceValue() { return ++uploadFenceValue_; }
```

---

#### Step 4: Update Texture Upload Path

**File:** `src/igl/d3d12/Texture.cpp`

**Locate:** `upload` / `uploadInternal` methods

**Apply:** Same pattern as Step 3:
```cpp
// After ExecuteCommandLists for texture upload
uint64_t uploadFenceValue = device_->getNextUploadFenceValue();
uploadQueue->Signal(device_->getUploadFence(), uploadFenceValue);

// Track staging buffer
device_->trackUploadBuffer(stagingBuffer, uploadFenceValue);
```

---

#### Step 5: Verify Ring Buffer Uses Upload Fence

**File:** `src/igl/d3d12/UploadRingBuffer.cpp`

**Locate:** `retire` method

**Verify:**
```cpp
void UploadRingBuffer::retire(uint64_t fenceValue) {
  // This should receive upload fence values, not swap-chain fence
  while (!fenceValues_.empty() && fenceValues_.front() <= fenceValue) {
    // Retire allocation
    tail_ = offsets_.front();
    offsets_.pop_front();
    fenceValues_.pop_front();
  }
}
```

**Caller should pass:**
```cpp
// In Device::processCompletedUploads or similar
uint64_t completedValue = uploadFence_->GetCompletedValue();
uploadRingBuffer_->retire(completedValue);
```

---

#### Step 6: Add Defensive Logging

**File:** `src/igl/d3d12/Device.cpp`

**In `processCompletedUploads`:**
```cpp
void Device::processCompletedUploads() {
  std::lock_guard<std::mutex> lock(uploadMutex_);

  uint64_t completedValue = uploadFence_->GetCompletedValue();
  size_t beforeCount = pendingUploads_.size();

  auto it = pendingUploads_.begin();
  while (it != pendingUploads_.end()) {
    if (it->fenceValue <= completedValue) {
      IGL_LOG_INFO("Retiring upload buffer (fence %llu <= %llu)\n",
                   it->fenceValue, completedValue);
      it = pendingUploads_.erase(it);
    } else {
      ++it;
    }
  }

  size_t afterCount = pendingUploads_.size();
  if (beforeCount > 0 && afterCount == beforeCount) {
    IGL_LOG_WARNING("No uploads retired! completedValue=%llu, oldest pending=%llu\n",
                    completedValue,
                    pendingUploads_.empty() ? 0 : pendingUploads_.front().fenceValue);
  }
}
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 backend tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Specific memory/resource tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Buffer*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*Upload*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Resource*"
```

**Test Modifications Allowed:**
- ✅ Add D3D12 memory leak detection tests
- ✅ Add fence value validation tests
- ❌ **DO NOT modify cross-platform test expectations**

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all sessions (tests continuous upload/release cycles)
./test_all_sessions.bat

# Specific texture-heavy sessions:
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required (this is a backend bug fix)
- ❌ **DO NOT modify session logic**

### Memory Leak Test (New Test Required):

```cpp
TEST(D3D12UploadTest, StagingBufferRetirement) {
  auto device = createD3D12Device();

  // Baseline memory
  size_t initialPending = device->getPendingUploadCount();

  // Upload 50 large textures
  for (int i = 0; i < 50; i++) {
    TextureDesc desc = {...};
    std::vector<uint8_t> data(2048 * 2048 * 4);
    auto tex = device->createTexture(desc, data.data());
  }

  // Wait for GPU to complete uploads
  device->waitForIdle();

  // Process retirements
  device->processCompletedUploads();

  // Verify all staging buffers retired
  size_t finalPending = device->getPendingUploadCount();
  EXPECT_EQ(finalPending, initialPending)
      << "Staging buffers leaked! Pending count grew from "
      << initialPending << " to " << finalPending;
}
```

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] New memory leak test passes
- [ ] `pendingUploads_` drains to 0 after uploads complete
- [ ] No live upload resources in PIX capture after frame
- [ ] Upload ring buffer retires allocations correctly
- [ ] No D3D12 debug layer warnings about resource lifetimes

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Memory leak test passes
2. All render sessions pass without memory growth
3. PIX shows no lingering upload resources

**Post in chat:**
```
DX12-NEW-02 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Memory leak test: PASS (pendingUploads_ = 0)
- PIX validation: No leaked upload resources

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- **G-002** (Excessive waitForGPU calls) - Can't optimize until fence handling is correct
- **B-003** (Ring buffer race) - Ring buffer retirement depends on correct fence values

### Related:
- **DX12-NEW-03** (Upload waits for GPU) - Same upload path, related synchronization issue

---

## Implementation Priority

**Priority:** P0 - HIGH (Memory Leak)
**Estimated Effort:** 4-6 hours
**Risk:** Medium (touches multiple upload paths)
**Impact:** HIGH - Prevents memory exhaustion in production

---

## References

- [Uploading Resources - Synchronize using Fences](https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources#synchronize-using-fences)
- [MiniEngine Upload Pattern](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicUploadHeap.cpp)
- [ID3D12Fence Interface](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12fence)
- [D3D12 Memory Management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
