# TASK_P0_DX12-005: Fix Upload Buffer Command Allocator Synchronization

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 4-6 hours
**Status:** Open

---

## Problem Statement

The D3D12 backend does not properly synchronize command allocators used for upload buffer operations with GPU completion. Command allocators are reused or destroyed before the GPU finishes executing commands, leading to device removal, corruption of upload data, and GPU crashes. This is a critical synchronization bug affecting resource upload reliability.

### Current Behavior
- Upload operations use command allocators from a pool or single allocator
- Command allocators recycled/destroyed without waiting for GPU completion
- No fence tracking for upload command allocator lifetime
- Race condition: CPU recycles allocator while GPU still executing commands
- Results in:
  - `DXGI_ERROR_DEVICE_REMOVED`
  - Corrupted texture/buffer uploads
  - GPU page faults
  - Undefined behavior

### Expected Behavior
- Each command allocator tracked with fence value indicating last usage
- Command allocators only recycled after GPU completes commands (fence signaled)
- Proper fence-based synchronization for upload queue
- Safe reuse of command allocators across frames

---

## Evidence and Code Location

### Where to Find the Issue

**Primary Files:**
- `src/igl/d3d12/Buffer.cpp` (upload operations)
- `src/igl/d3d12/Texture.cpp` (texture upload)
- `src/igl/d3d12/Device.cpp` (command allocator management)
- `src/igl/d3d12/CommandQueue.cpp` (upload queue handling)

**Search Pattern 1 - Command Allocator Recycling:**

1. Find command allocator allocation/recycling logic:
```cpp
// Search for:
commandAllocator->Reset();
// ← PROBLEM: No check if GPU finished using this allocator
```

2. Look for command allocator pool or management:
```cpp
// Pattern to find:
ComPtr<ID3D12CommandAllocator> allocator;
getCommandAllocator(&allocator); // Gets allocator from pool
// ← Need to track: Is this allocator free (GPU done)?
```

**Search Pattern 2 - Upload Operations Without Fence Tracking:**

1. Find upload buffer operations:
```cpp
// In Buffer::upload() or similar:
device_->createUploadCommandList();
commandList->CopyBufferRegion(...);
commandList->Close();
commandQueue->ExecuteCommandLists(...);
// ← PROBLEM: No fence signaled, allocator can be reused immediately
```

2. Search for texture upload operations:
```cpp
// In Texture::upload() or similar:
uploadCommandList->CopyTextureRegion(...);
uploadQueue->ExecuteCommandLists(...);
// ← Missing: Signal fence and track allocator lifetime
```

**Search Pattern 3 - Missing Fence Synchronization:**

Look for fence usage in upload paths:
```cpp
// Should have pattern like:
commandQueue->Signal(uploadFence.Get(), ++uploadFenceValue);
// But might be missing or incorrect
```

**Search Keywords:**
- `ID3D12CommandAllocator`
- `commandAllocator->Reset()`
- `ExecuteCommandLists` (in upload context)
- `CopyBufferRegion`, `CopyTextureRegion`
- Upload queue operations
- Command allocator pool/management

---

## Impact

**Severity:** Critical - Causes device removal and crashes
**Reliability:** Upload operations unreliable
**Data Integrity:** Corrupted resource data

**Affected Code Paths:**
- All buffer upload operations (`Buffer::upload()`)
- All texture upload operations (`Texture::upload()`)
- Dynamic buffer updates
- Streaming texture updates
- Immediate uploads during rendering

**Symptoms:**
- Device removal during/after upload operations
- Corrupted texture data (missing/wrong pixels)
- Corrupted buffer data (incorrect vertex/uniform data)
- Intermittent crashes in upload-heavy scenes
- More frequent in high-upload scenarios (streaming, dynamic resources)

**Risk Level:** GPU crash risk - can damage driver stability

---

## Official References

### Microsoft Documentation

1. **Command Allocator Reuse** - [Reusing Command Allocators](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles#re-using-command-allocators)
   - "Command allocators can be re-used only after the GPU completes execution of all commands"
   - "Use fences to determine when the GPU has finished executing commands"

2. **Command Allocator Reset** - [ID3D12CommandAllocator::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset)
   - "You must ensure the GPU is no longer executing commands before calling Reset"
   - Violation causes `E_FAIL` or device removal

3. **Synchronization with Fences** - [Using Fences](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
   ```cpp
   // Signal fence after command execution
   commandQueue->Signal(fence, fenceValue);

   // Wait for GPU completion before reusing allocator
   if (fence->GetCompletedValue() < fenceValue) {
       fence->SetEventOnCompletion(fenceValue, eventHandle);
       WaitForSingleObject(eventHandle, INFINITE);
   }

   // Now safe to reset allocator
   commandAllocator->Reset();
   ```

4. **Device Removal** - [DXGI_ERROR_DEVICE_REMOVED](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error)
   - Common cause: Reusing command allocator too early

### DirectX-Graphics-Samples

- **D3D12HelloTriangle** - Basic fence synchronization example
- **D3D12Bundles** - Command allocator reuse pattern
- **D3D12nBodyGravity** - Per-frame command allocator management

### Best Practices

1. **Per-Frame Command Allocators:**
   - Allocate N command allocators (N = number of buffered frames, typically 2-3)
   - Rotate allocators per frame
   - Track each allocator's fence value
   - Reset only when fence signaled

2. **Immediate Upload Pattern:**
   - Create temporary command allocator for upload
   - Signal fence after upload completes
   - Wait for fence before destroying/recycling allocator

3. **Command Allocator Pool:**
   - Maintain pool of allocators with associated fence values
   - `getAvailableAllocator()` checks fence completion
   - Return allocator to pool with current fence value
   - Reuse only if fence signaled

---

## Implementation Guidance

### High-Level Approach

**Recommended: Fence-Tracked Command Allocator Pool**

1. **Create Command Allocator Tracking Structure**
   - Associate each command allocator with fence value
   - Track which queue used allocator (copy queue for uploads)
   - Store allocators in pool with metadata

2. **Modify Upload Operations**
   - Get command allocator from pool (ensures GPU completed previous use)
   - Record upload commands
   - Execute commands
   - **Signal fence** with new fence value
   - Return allocator to pool with fence value

3. **Implement Pool Management**
   - When requesting allocator: Check if any allocator's fence is signaled
   - If no allocators available, either wait or allocate new one
   - Lazy cleanup: Check fences when requesting allocators

### Detailed Steps

**Step 1: Define Command Allocator Tracking Structure**

Location: `src/igl/d3d12/Device.h` or new `CommandAllocatorPool.h`

```cpp
struct TrackedCommandAllocator {
    ComPtr<ID3D12CommandAllocator> allocator;
    uint64_t fenceValue = 0;  // Fence value when last used
    D3D12_COMMAND_LIST_TYPE type;
};

class CommandAllocatorPool {
public:
    ComPtr<ID3D12CommandAllocator> getAvailableAllocator(
        ID3D12Device* device,
        ID3D12Fence* fence,
        D3D12_COMMAND_LIST_TYPE type
    );

    void returnAllocator(
        ComPtr<ID3D12CommandAllocator> allocator,
        uint64_t fenceValue,
        D3D12_COMMAND_LIST_TYPE type
    );

private:
    std::vector<TrackedCommandAllocator> allocators_;
    std::mutex mutex_; // Thread safety if needed
};
```

**Step 2: Implement Pool Get/Return Logic**

```cpp
ComPtr<ID3D12CommandAllocator> CommandAllocatorPool::getAvailableAllocator(
    ID3D12Device* device,
    ID3D12Fence* fence,
    D3D12_COMMAND_LIST_TYPE type
) {
    // Check if any existing allocator is available (fence signaled)
    for (auto& tracked : allocators_) {
        if (tracked.type != type) continue;

        uint64_t completedValue = fence->GetCompletedValue();
        if (completedValue >= tracked.fenceValue) {
            // GPU finished using this allocator, safe to reuse
            ComPtr<ID3D12CommandAllocator> allocator = tracked.allocator;

            // Remove from pool (will be returned later)
            tracked = allocators_.back();
            allocators_.pop_back();

            // Reset allocator
            HRESULT hr = allocator->Reset();
            if (FAILED(hr)) {
                IGL_LOG_ERROR("D3D12: CommandAllocator::Reset failed: 0x%08X\n", hr);
                return nullptr;
            }

            return allocator;
        }
    }

    // No available allocator, create new one
    ComPtr<ID3D12CommandAllocator> newAllocator;
    HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&newAllocator));
    if (FAILED(hr)) {
        IGL_LOG_ERROR("D3D12: CreateCommandAllocator failed: 0x%08X\n", hr);
        return nullptr;
    }

    IGL_LOG_DEBUG("D3D12: Created new command allocator (pool size: %zu)\n", allocators_.size() + 1);
    return newAllocator;
}

void CommandAllocatorPool::returnAllocator(
    ComPtr<ID3D12CommandAllocator> allocator,
    uint64_t fenceValue,
    D3D12_COMMAND_LIST_TYPE type
) {
    TrackedCommandAllocator tracked;
    tracked.allocator = allocator;
    tracked.fenceValue = fenceValue;
    tracked.type = type;

    allocators_.push_back(tracked);
}
```

**Step 3: Integrate Pool into Device or Upload Manager**

Location: `src/igl/d3d12/Device.h`

```cpp
class Device {
private:
    CommandAllocatorPool commandAllocatorPool_;
    ComPtr<ID3D12Fence> uploadFence_;
    uint64_t uploadFenceValue_ = 0;

public:
    // Expose upload fence for synchronization
    ID3D12Fence* getUploadFence() { return uploadFence_.Get(); }
    uint64_t getNextUploadFenceValue() { return ++uploadFenceValue_; }
};
```

**Step 4: Modify Upload Operations to Use Pool**

Location: `src/igl/d3d12/Buffer.cpp` - `Buffer::upload()` or similar

Before (UNSAFE):
```cpp
void Buffer::upload(const void* data, size_t size) {
    ComPtr<ID3D12CommandAllocator> allocator;
    device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&allocator));

    ComPtr<ID3D12GraphicsCommandList> commandList;
    device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, allocator.Get(), nullptr,
                                IID_PPV_ARGS(&commandList));

    commandList->CopyBufferRegion(...);
    commandList->Close();

    uploadQueue->ExecuteCommandLists(...);

    // ← PROBLEM: Allocator can be destroyed/reused immediately, GPU might still be using it
}
```

After (SAFE):
```cpp
void Buffer::upload(const void* data, size_t size) {
    // Get safe allocator from pool (ensures GPU completed previous use)
    ComPtr<ID3D12CommandAllocator> allocator =
        device_->getCommandAllocatorPool()->getAvailableAllocator(
            device_->getDevice(),
            device_->getUploadFence(),
            D3D12_COMMAND_LIST_TYPE_COPY
        );

    if (!allocator) {
        IGL_LOG_ERROR("Failed to get command allocator for upload\n");
        return;
    }

    ComPtr<ID3D12GraphicsCommandList> commandList;
    HRESULT hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, allocator.Get(),
                                             nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) {
        IGL_LOG_ERROR("CreateCommandList failed: 0x%08X\n", hr);
        device_->getCommandAllocatorPool()->returnAllocator(allocator, 0, D3D12_COMMAND_LIST_TYPE_COPY);
        return;
    }

    // Record upload commands
    commandList->CopyBufferRegion(...);
    commandList->Close();

    // Execute
    ID3D12CommandList* cmdLists[] = { commandList.Get() };
    uploadQueue->ExecuteCommandLists(1, cmdLists);

    // ✓ CRITICAL: Signal fence so we know when GPU finishes
    uint64_t fenceValue = device_->getNextUploadFenceValue();
    uploadQueue->Signal(device_->getUploadFence(), fenceValue);

    // Return allocator to pool with fence value (will be reused after fence signaled)
    device_->getCommandAllocatorPool()->returnAllocator(allocator, fenceValue,
                                                         D3D12_COMMAND_LIST_TYPE_COPY);
}
```

**Step 5: Initialize Upload Fence in Device Creation**

Location: `src/igl/d3d12/Device.cpp` - Device initialization

```cpp
void Device::initialize() {
    // ... existing initialization ...

    // Create fence for upload synchronization
    HRESULT hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence_));
    if (FAILED(hr)) {
        IGL_LOG_ERROR("D3D12: Failed to create upload fence: 0x%08X\n", hr);
        // Handle error...
    }

    uploadFenceValue_ = 0;
}
```

**Step 6: Apply Same Pattern to Texture Upload**

Location: `src/igl/d3d12/Texture.cpp` - `Texture::upload()` or similar

Apply same pattern as buffer upload:
1. Get allocator from pool
2. Record copy commands
3. Execute
4. Signal fence
5. Return allocator to pool with fence value

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- Buffer upload tests
- Texture upload tests
- Resource creation tests

**Render Sessions:**
```bash
test_all_sessions.bat
```
Critical sessions with uploads:
- **All textured sessions** (texture uploads)
- **DrawInstancedSession** (buffer uploads)
- **Any session with dynamic buffers**
- All other sessions (regression check)

### Validation Steps

1. **Device Removal Testing**
   - Run all tests multiple times
   - Should complete without `DXGI_ERROR_DEVICE_REMOVED`
   - Check for device removal: `device->GetDeviceRemovedReason()` should return `S_OK`

2. **Upload Stress Testing**
   - Create test that uploads many textures/buffers rapidly
   - Verify no crashes or corruption
   - Monitor command allocator pool size (should stabilize, not grow indefinitely)

3. **Debug Layer Validation**
   ```bash
   # Enable D3D12 debug layer
   # Run tests
   # Check for errors:
   # - "Command allocator cannot be reset while commands are still executing"
   # - "Invalid command allocator state"
   ```

4. **Fence Verification**
   - Add logging to verify fence signaling happens
   - Verify fence values increment correctly
   - Verify allocators only reused after fence signaled

5. **Regression Testing**
   - All tests pass
   - No visual differences
   - No performance degradation

### Long-Running Test

Critical for this bug:
```bash
# Run session in loop for extended period
for i in {1..100}; do
    echo "Iteration $i"
    ./Textured3DCubeSession_d3d12.exe --headless --screenshot-number 0
done
```
Should complete 100 iterations without device removal.

---

## Success Criteria

- [ ] Command allocator pool implemented with fence tracking
- [ ] Upload operations get allocators from pool
- [ ] Fence signaled after each upload operation
- [ ] Allocators returned to pool with fence value
- [ ] Allocators only reused after fence signaled (GPU completed)
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] No device removal errors in any test
- [ ] No validation layer errors about command allocator state
- [ ] Stress test (100+ uploads) completes without issues
- [ ] Debug layer validation passes
- [ ] User confirms tests pass, especially stress tests

---

## Dependencies

**Depends On:**
- None (critical bug fix)

**Related:**
- TASK_P1_DX12-FIND-03 (Schedule Fence Monotonic) - Fence management improvements
- TASK_P1_DX12-009 (Upload Ring Buffer) - May integrate with upload improvements
- TASK_P2_DX12-FIND-07 (Upload GPU Wait) - Sync optimization after this fix

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts
   - ❌ DO NOT modify render sessions
   - ✅ Tests must pass without device removal

2. **User Confirmation Required**
   - ⚠️ Run stress tests (100+ uploads) - **MANDATORY**
   - ⚠️ Run with D3D12 debug layer enabled
   - ⚠️ Report device removed reason (should be S_OK)
   - ⚠️ Wait for user confirmation

3. **Code Modification Scope**
   - ✅ Modify `Device.cpp`, `Device.h`
   - ✅ Modify `Buffer.cpp`, `Texture.cpp` upload paths
   - ✅ Create `CommandAllocatorPool` class/helpers
   - ❌ DO NOT change upload queue creation (unless necessary)
   - ❌ DO NOT modify IGL public API

4. **Thread Safety**
   - Consider multi-threaded upload scenarios
   - Add mutex to pool if concurrent access possible
   - Test thread safety if applicable

---

**Estimated Timeline:** 4-6 hours
**Risk Level:** Medium-High (critical path, requires careful synchronization)
**Validation Effort:** 3-4 hours (stress testing required)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
