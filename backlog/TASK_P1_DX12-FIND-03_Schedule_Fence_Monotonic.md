# TASK_P1_DX12-FIND-03: Fix Non-Monotonic Schedule Fence Values

**Priority:** P1 - High
**Estimated Effort:** 3-4 hours
**Status:** Open
**Issue ID:** DX12-FIND-03 (from Codex Audit)

---

## Problem Statement

The `CommandQueue::submit()` function signals the `scheduleFence_` with a fixed value of `1` on every submission, violating D3D12's requirement for monotonically increasing fence values. This breaks the `CommandBuffer::waitUntilScheduled()` function after the first submission, as the fence value never changes and subsequent waits complete immediately without actual synchronization.

### Current Behavior
- Every call to `CommandQueue::submit()` executes:
  ```cpp
  d3dCommandQueue->Signal(scheduleFence_.Get(), 1);  // Always value 1!
  ```
- First submission signals fence to value `1`
- Second submission signals fence to value `1` again (fence already at `1`)
- `CommandBuffer::waitUntilScheduled()` waits for fence value `1`
- Since fence is already at `1`, wait completes immediately
- **Result:** No actual synchronization after first frame

### Expected Behavior
- Fence value increments on each signal operation
- Pattern should be:
  ```cpp
  fenceValue++;
  d3dCommandQueue->Signal(scheduleFence_.Get(), fenceValue);
  ```
- Each command buffer tracks its submission fence value
- `waitUntilScheduled()` waits for the specific fence value associated with that submission

---

## Evidence and Code Location

### Where to Find the Issue

**Primary File:** `src/igl/d3d12/CommandQueue.cpp`

**Search Pattern:**
1. Locate function `CommandQueue::submit()`
2. Find the `Signal` call on `scheduleFence_`
3. Observe the hard-coded value `1`:
```cpp
// Search for this exact pattern:
d3dCommandQueue->Signal(scheduleFence_.Get(), 1);
// or similar patterns like:
commandQueue_->Signal(scheduleFence_.Get(), 1);
```

**Line Reference (from Codex):** Around line 142-144 in `CommandQueue.cpp`

**Related Code:**
- `CommandBuffer::waitUntilScheduled()` - Consumes the fence value
- `CommandQueue` class initialization - Where `scheduleFence_` is created
- May need to add member variable to track fence value

**Search Keywords:**
- `scheduleFence_`
- `Signal` (within CommandQueue context)
- `waitUntilScheduled`
- `ID3D12Fence::Signal`

---

## Impact

**Severity:** High - Breaks synchronization contract
**Reliability:** Timeline synchronization features unusable
**Correctness:** Commands may execute out of order

**Affected Features:**
- `CommandBuffer::waitUntilScheduled()` - Only works for first submission
- Any code relying on command buffer completion tracking
- Future timeline synchronization features
- Multi-queue synchronization (if implemented)

**Symptoms:**
- Race conditions in multi-threaded scenarios
- Commands executed before dependencies complete
- Intermittent rendering artifacts
- May work "by accident" due to other synchronization (frame fences, GPU waits)

**Why This Matters:**
- IGL contract promises `waitUntilScheduled()` functionality
- Cross-API portability requires consistent synchronization semantics
- Future features (async compute, multi-queue) will depend on correct fence usage

---

## Official References

### Microsoft Documentation

1. **Fence Synchronization** - [Using Fence Values](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
   - "Fence values must be monotonically increasing"
   - "Each Signal operation should use a new, higher value"

2. **ID3D12CommandQueue::Signal** - [API Reference](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal)
   ```cpp
   HRESULT Signal(
     ID3D12Fence *pFence,
     UINT64      Value  // Must be greater than previous value
   );
   ```
   - "The value must be greater than the current fence value"

3. **Synchronization Best Practices** - [Fence Usage Pattern](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)
   ```cpp
   // Correct pattern:
   const UINT64 fenceValueForSignal = ++m_fenceValue;
   ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValueForSignal));
   ```

### DirectX-Graphics-Samples

- **D3D12HelloTriangle** - Basic fence pattern:
  ```cpp
  const UINT64 fence = m_fenceValue;
  m_commandQueue->Signal(m_fence.Get(), fence);
  m_fenceValue++;
  ```

- **D3D12Multithreading** - Advanced fence usage with per-thread counters
- **D3D12nBodyGravity** - Per-frame fence tracking

### Cross-Reference

- **Vulkan Backend:** Uses `VkSemaphore` timeline semaphores with monotonic values
- **Metal Backend:** Uses `MTLFence` with proper signaling

---

## Implementation Guidance

### High-Level Approach

1. **Add Fence Value Counter**
   - Add `uint64_t scheduleFenceValue_` member to `CommandQueue` class
   - Initialize to `0` in constructor

2. **Increment on Each Signal**
   - Before calling `Signal()`, increment the counter
   - Pass the new value to `Signal()`

3. **Store Fence Value in Command Buffer**
   - When submitting command buffer, store the fence value
   - `CommandBuffer` needs member to track its submission fence value

4. **Use Stored Value in waitUntilScheduled()**
   - Wait for the specific fence value associated with this command buffer

### Detailed Steps

**Step 1: Add Fence Value Member to CommandQueue**

Location: `src/igl/d3d12/CommandQueue.h`

```cpp
class CommandQueue {
private:
    ComPtr<ID3D12Fence> scheduleFence_;
    uint64_t scheduleFenceValue_ = 0;  // ← ADD THIS
    // ... other members ...
};
```

**Step 2: Initialize Fence Value in Constructor**

Location: `src/igl/d3d12/CommandQueue.cpp` - Constructor

```cpp
CommandQueue::CommandQueue(...)
    : scheduleFenceValue_(0) {  // ← ADD INITIALIZATION
    // ... existing initialization ...

    // Create fence (if not already present)
    HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&scheduleFence_));
    // ... error handling ...
}
```

**Step 3: Increment Fence Value in submit()**

Location: `src/igl/d3d12/CommandQueue.cpp` - `submit()` function

Before (INCORRECT):
```cpp
void CommandQueue::submit(const CommandBuffer& commandBuffer) {
    // ... execute command lists ...

    // Signal fence
    d3dCommandQueue->Signal(scheduleFence_.Get(), 1);  // ← WRONG: Always 1
}
```

After (CORRECT):
```cpp
void CommandQueue::submit(const CommandBuffer& commandBuffer) {
    // ... execute command lists ...

    // Signal fence with monotonically increasing value
    ++scheduleFenceValue_;  // ← INCREMENT FIRST
    HRESULT hr = d3dCommandQueue_->Signal(scheduleFence_.Get(), scheduleFenceValue_);

    if (FAILED(hr)) {
        IGL_LOG_ERROR("D3D12: CommandQueue::Signal failed with HRESULT 0x%08X\n", hr);
        // Handle error...
    }

    // Store fence value in command buffer for waitUntilScheduled()
    // (see Step 4)
    const_cast<CommandBuffer&>(commandBuffer).setScheduleFenceValue(scheduleFenceValue_);
}
```

**Step 4: Add Fence Value Tracking to CommandBuffer**

Location: `src/igl/d3d12/CommandBuffer.h`

```cpp
class CommandBuffer {
private:
    uint64_t scheduledFenceValue_ = 0;  // ← ADD THIS

public:
    void setScheduleFenceValue(uint64_t value) {
        scheduledFenceValue_ = value;
    }

    uint64_t getScheduleFenceValue() const {
        return scheduledFenceValue_;
    }

    // ... existing methods ...
};
```

**Step 5: Update waitUntilScheduled() Implementation**

Location: `src/igl/d3d12/CommandBuffer.cpp`

Before (BROKEN):
```cpp
void CommandBuffer::waitUntilScheduled() {
    // Waits for fence value 1 (only works first time)
    if (scheduleFence_->GetCompletedValue() < 1) {
        scheduleFence_->SetEventOnCompletion(1, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}
```

After (CORRECT):
```cpp
void CommandBuffer::waitUntilScheduled() {
    // If not yet submitted, nothing to wait for
    if (scheduledFenceValue_ == 0) {
        return;
    }

    // Wait for the specific fence value from this buffer's submission
    if (scheduleFence_->GetCompletedValue() < scheduledFenceValue_) {
        HRESULT hr = scheduleFence_->SetEventOnCompletion(scheduledFenceValue_, fenceEvent_);
        if (FAILED(hr)) {
            IGL_LOG_ERROR("D3D12: SetEventOnCompletion failed: 0x%08X\n", hr);
            return;
        }

        DWORD result = WaitForSingleObject(fenceEvent_, INFINITE);
        if (result != WAIT_OBJECT_0) {
            IGL_LOG_ERROR("D3D12: WaitForSingleObject failed: %u\n", result);
        }
    }
}
```

**Step 6: Optional - Add Diagnostic Logging**

```cpp
void CommandQueue::submit(const CommandBuffer& commandBuffer) {
    // ... execute commands ...

    ++scheduleFenceValue_;
    d3dCommandQueue_->Signal(scheduleFence_.Get(), scheduleFenceValue_);

    IGL_LOG_DEBUG("D3D12: Signaled schedule fence to value %llu\n", scheduleFenceValue_);

    // ... rest of submit ...
}
```

### Edge Cases and Considerations

**Thread Safety:**
- If `CommandQueue::submit()` can be called from multiple threads, add mutex around fence value increment
- Consider using `std::atomic<uint64_t>` for `scheduleFenceValue_`

**Overflow:**
- `uint64_t` provides 2^64 values - effectively infinite for this use case
- At 1 million submissions per second, would take ~584 million years to overflow

**Fence Event Handle:**
- Ensure `fenceEvent_` is created (typically in `CommandBuffer` constructor)
- Pattern:
  ```cpp
  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  ```

**Multiple Command Queues:**
- If multiple queue types exist (graphics, compute, copy), each should have its own fence and counter
- This implementation assumes single queue or separate fences per queue type

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- All command queue tests
- All command buffer tests
- Synchronization tests (if they exist)

**Render Sessions:**
```bash
test_all_sessions.bat
```
All sessions must pass (no regressions):
- BasicFramebufferSession
- HelloTriangleSession
- HelloWorldSession
- ThreeCubesRenderSession
- All other sessions

### Validation Steps

1. **Fence Value Verification**
   - Add logging to verify fence values increment: 1, 2, 3, 4, ...
   - Run any session, check logs show monotonic increase
   - Verify no repeated fence values

2. **waitUntilScheduled() Testing**
   - Create test that submits multiple command buffers
   - Call `waitUntilScheduled()` on each
   - Verify waits complete correctly (not immediately)
   - Check fence completed value matches expected

3. **Multi-Submission Test**
   ```cpp
   // Pseudocode test:
   for (int i = 0; i < 100; i++) {
       auto cmdBuf = createCommandBuffer();
       // ... record commands ...
       commandQueue.submit(cmdBuf);
       cmdBuf.waitUntilScheduled();  // Should wait for GPU completion
   }
   // Verify 100 different fence values signaled
   ```

4. **Debug Layer Validation**
   - Run with D3D12 debug layer enabled
   - No errors related to fence values
   - No warnings about fence synchronization

5. **Regression Testing**
   - All existing tests pass
   - No performance degradation
   - No new crashes or hangs

---

## Success Criteria

- [ ] `scheduleFenceValue_` member added to `CommandQueue` class
- [ ] Fence value initialized to `0` in constructor
- [ ] `submit()` increments fence value before each `Signal()` call
- [ ] Fence value passed to `Signal()` is the incremented value
- [ ] `CommandBuffer` stores its submission fence value
- [ ] `waitUntilScheduled()` waits for stored fence value (not hardcoded `1`)
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] Logging shows monotonic fence values (1, 2, 3, ...)
- [ ] No fence-related validation errors
- [ ] `waitUntilScheduled()` verified to actually wait (not return immediately)
- [ ] User confirms tests pass and fence behavior is correct

---

## Dependencies

**Depends On:**
- None (isolated bug fix)

**Enables:**
- Future timeline synchronization features
- Multi-queue synchronization
- Async compute features
- Proper command buffer tracking

**Related:**
- TASK_P0_DX12-005 (Upload Buffer Sync) - Similar fence tracking pattern
- Future tasks involving cross-queue synchronization

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts
   - ❌ DO NOT modify render sessions
   - ✅ Tests must pass with new fence behavior

2. **User Confirmation Required**
   - ⚠️ Verify fence values increment monotonically (check logs)
   - ⚠️ Test `waitUntilScheduled()` actually waits
   - ⚠️ Report fence value progression
   - ⚠️ Wait for user confirmation

3. **Code Modification Scope**
   - ✅ Modify `CommandQueue.cpp`, `CommandQueue.h`
   - ✅ Modify `CommandBuffer.cpp`, `CommandBuffer.h`
   - ✅ Add diagnostic logging
   - ❌ DO NOT change fence creation (unless necessary)
   - ❌ DO NOT modify IGL public API

4. **Backward Compatibility**
   - New fence behavior should not break existing code
   - Waits should still complete (just correctly now)
   - No API changes to IGL contract

---

**Estimated Timeline:** 3-4 hours
**Risk Level:** Low-Medium (isolated change, well-defined fix)
**Validation Effort:** 2 hours (verify fence progression, test waits)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 03*
