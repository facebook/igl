# TASK_P0_DX12-FIND-02: Fix Descriptor Heap Overflow and Missing Bounds Checks

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 2-3 days
**Status:** Open
**Issue ID:** DX12-FIND-02 (from Codex Audit)

---

## Problem Statement

The D3D12 backend allocates descriptors from fixed-size descriptor heaps without bounds checking. When descriptor allocation exceeds heap capacity (typically 1024 descriptors for CBV/SRV/UAV heaps), the backend overflows heap boundaries, causing memory corruption, undefined behavior, and device removal. This is particularly problematic in headless mode and long-running sessions where descriptor allocation is cumulative.

### Current Behavior
- Descriptor heaps allocated with fixed size (e.g., 1024 descriptors)
- `getNextCbvSrvUavDescriptor()` returns incrementing index without bounds checking
- When index exceeds heap capacity:
  - Writes to invalid descriptor memory
  - Causes D3D12 validation errors
  - Leads to device removal (`DXGI_ERROR_DEVICE_REMOVED`)
  - Memory corruption in descriptor heap
- **Crashes automated tests and headless sessions**

### Expected Behavior
- Descriptor allocation includes bounds checking
- When heap is full, either:
  - Return error/null descriptor with proper error handling
  - Allocate new descriptor heap and switch to it
  - Use ring buffer pattern to recycle descriptors
- Graceful degradation instead of crashes
- Telemetry to warn when approaching heap limits

---

## Evidence and Code Location

### Where to Find the Issue

**Primary File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Search Pattern 1 - Descriptor Allocation:**
1. Locate function `bindTexture()` or similar binding functions
2. Find call to `commandBuffer_.getNextCbvSrvUavDescriptor()`
3. Notice the returned index is used directly without validation
4. Search for pattern:
```cpp
uint32_t descriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor()++;
// ← PROBLEM: No check if descriptorIndex < heapSize
```

**Search Pattern 2 - Heap Initialization:**
1. Locate `CommandBuffer` class or descriptor heap management
2. Find descriptor heap creation (D3D12_DESCRIPTOR_HEAP_DESC)
3. Find heap size initialization:
```cpp
heapDesc.NumDescriptors = 1024; // Fixed size, no growth
```

**Search Pattern 3 - Index Management:**
1. Find member variable tracking current descriptor index
2. Look for increment operations without bounds checking
3. Search for pattern like:
```cpp
uint32_t nextDescriptorIndex_;
// Incremented without checking against NumDescriptors
```

**Related Files:**
- `src/igl/d3d12/CommandBuffer.h` / `.cpp` - Heap management
- `src/igl/d3d12/RenderCommandEncoder.cpp` - Descriptor binding
- `src/igl/d3d12/ComputeCommandEncoder.cpp` - Compute descriptor binding

### How to Reproduce
1. Run any render session in headless mode with many textures/resources
2. Use `--headless` flag or run in CI environment
3. Bind >1024 unique descriptors in a single command buffer
4. Device removal occurs with error `DXGI_ERROR_DEVICE_REMOVED`
5. Debug layer shows: "Descriptor heap index out of bounds"

---

## Impact

**Severity:** Critical - Causes crashes and device removal
**Reliability:** Breaks automated testing and production use
**Memory Safety:** Memory corruption risk

**Affected Code Paths:**
- All texture binding operations
- All buffer binding operations
- Any command buffer with >1024 descriptor allocations
- Headless/automated testing (accumulates descriptors without clearing)
- Long-running sessions

**Symptoms:**
- Device removal errors
- Validation layer: "Invalid descriptor heap index"
- Crashes in multi-texture scenes
- Intermittent failures in CI/CD

**Blocks:**
- TASK_P1_DX12-FIND-05 (Storage Buffer Binding) - Needs safe descriptor allocation
- TASK_P3_DX12-FIND-13 (Multi-Draw Indirect) - Heavy descriptor usage
- Production deployment
- Automated testing reliability

---

## Official References

### Microsoft Documentation

1. **Descriptor Heaps** - [Creating Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
   - "Applications must manage descriptor heap capacity"
   - Heap size specified at creation time

2. **CreateDescriptorHeap** - [API Reference](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)
   ```cpp
   typedef struct D3D12_DESCRIPTOR_HEAP_DESC {
     D3D12_DESCRIPTOR_HEAP_TYPE Type;
     UINT NumDescriptors; // Fixed size, cannot be exceeded
     D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
     UINT NodeMask;
   } D3D12_DESCRIPTOR_HEAP_DESC;
   ```

3. **Descriptor Heap Best Practices** - [Resource Binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding)
   - "Implement descriptor heap overflow protection"
   - "Use multiple heaps or ring buffer allocation"

4. **Debug Layer Validation** - [D3D12 Debug Layer](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer)
   - Detects out-of-bounds descriptor access
   - Error: `D3D12 ERROR: ID3D12Device::CreateShaderResourceView: Invalid descriptor handle`

### DirectX-Graphics-Samples

- **D3D12Bundles** - Shows descriptor heap management
- **D3D12ExecuteIndirect** - Demonstrates large descriptor usage patterns
- **D3D12nBodyGravity** - Multiple descriptor heap switching

### Industry Best Practices

1. **Ring Buffer Allocation:**
   - Allocate descriptors from circular buffer
   - Recycle descriptors after frame completion
   - Used by Unreal Engine, Unity

2. **Dynamic Heap Growth:**
   - Allocate new heap when current is full
   - Maintain array of heaps
   - Switch heap via `SetDescriptorHeaps()`

3. **Descriptor Budgeting:**
   - Track usage per frame
   - Warn when approaching limits
   - Fail gracefully, not with crashes

---

## Implementation Guidance

### High-Level Approach

**Recommended: Ring Buffer Allocation Strategy**

1. **Per-Frame Descriptor Heaps**
   - Allocate descriptor heap per frame (or per command buffer)
   - Reset allocation index at frame boundary
   - Size heap based on maximum expected usage

2. **Bounds Checking**
   - Before allocating descriptor, check: `nextIndex < heapSize`
   - If full, either:
     - **Option A:** Return error, fail gracefully
     - **Option B:** Allocate additional heap, switch context
     - **Option C:** Flush command buffer, reset heap

3. **Heap Switching (Advanced)**
   - Maintain pool of descriptor heaps
   - When current heap is full, allocate new heap
   - Use `ID3D12GraphicsCommandList::SetDescriptorHeaps()` to switch

4. **Telemetry**
   - Track descriptor usage statistics
   - Warn when usage exceeds 80% of capacity
   - Log heap size and allocation patterns

### Detailed Steps

**Step 1: Add Bounds Checking to Descriptor Allocation**

Location: `src/igl/d3d12/CommandBuffer.cpp` (or wherever `getNextCbvSrvUavDescriptor()` is defined)

Current (unsafe):
```cpp
uint32_t CommandBuffer::getNextCbvSrvUavDescriptor() {
    return nextCbvSrvUavDescriptor_++;
}
```

Fixed (with bounds check):
```cpp
uint32_t CommandBuffer::getNextCbvSrvUavDescriptor() {
    IGL_ASSERT_MSG(nextCbvSrvUavDescriptor_ < cbvSrvUavHeapSize_,
                   "Descriptor heap overflow! Allocated: %u, Capacity: %u",
                   nextCbvSrvUavDescriptor_, cbvSrvUavHeapSize_);

    if (nextCbvSrvUavDescriptor_ >= cbvSrvUavHeapSize_) {
        IGL_LOG_ERROR("D3D12: Descriptor heap overflow, returning invalid descriptor\n");
        // Option 1: Return last valid descriptor (degraded behavior)
        return cbvSrvUavHeapSize_ - 1;

        // Option 2: Trigger heap growth (advanced, see Step 3)
        // allocateNewDescriptorHeap();
    }

    return nextCbvSrvUavDescriptor_++;
}
```

**Step 2: Add Heap Size Tracking**

Location: `src/igl/d3d12/CommandBuffer.h`

Add member variables:
```cpp
class CommandBuffer {
private:
    uint32_t cbvSrvUavHeapSize_ = 1024; // Current heap capacity
    uint32_t nextCbvSrvUavDescriptor_ = 0;
    uint32_t peakDescriptorUsage_ = 0; // For telemetry

    // Similarly for sampler heap if applicable
    uint32_t samplerHeapSize_ = 256;
    uint32_t nextSamplerDescriptor_ = 0;
};
```

**Step 3: Implement Descriptor Reset on Frame Boundary (Ring Buffer)**

Location: Command buffer reset function (likely `CommandBuffer::reset()` or similar)

```cpp
void CommandBuffer::reset() {
    // Reset descriptor allocation indices
    nextCbvSrvUavDescriptor_ = 0;
    nextSamplerDescriptor_ = 0;

    // Track peak usage for telemetry
    if (nextCbvSrvUavDescriptor_ > peakDescriptorUsage_) {
        peakDescriptorUsage_ = nextCbvSrvUavDescriptor_;

        if (peakDescriptorUsage_ > cbvSrvUavHeapSize_ * 0.8) {
            IGL_LOG_WARN("D3D12: Descriptor usage at %.1f%% capacity (%u/%u)\n",
                         (float)peakDescriptorUsage_ / cbvSrvUavHeapSize_ * 100.0f,
                         peakDescriptorUsage_, cbvSrvUavHeapSize_);
        }
    }

    // ... rest of reset logic
}
```

**Step 4: (Advanced) Implement Dynamic Heap Growth**

If ring buffer isn't sufficient, implement heap growth:

```cpp
void CommandBuffer::allocateNewDescriptorHeap() {
    // Create new heap with larger size
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = cbvSrvUavHeapSize_ * 2; // Double size
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> newHeap;
    HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&newHeap));

    if (SUCCEEDED(hr)) {
        // Switch to new heap
        cbvSrvUavHeap_ = newHeap;
        cbvSrvUavHeapSize_ = heapDesc.NumDescriptors;
        nextCbvSrvUavDescriptor_ = 0;

        // Update command list to use new heap
        ID3D12DescriptorHeap* heaps[] = { cbvSrvUavHeap_.Get(), samplerHeap_.Get() };
        commandList_->SetDescriptorHeaps(2, heaps);

        IGL_LOG_INFO("D3D12: Grew descriptor heap to %u descriptors\n", cbvSrvUavHeapSize_);
    }
}
```

**Step 5: Update Descriptor Binding Sites**

Location: `RenderCommandEncoder.cpp`, `ComputeCommandEncoder.cpp`

Everywhere descriptors are allocated, add error handling:

```cpp
void RenderCommandEncoder::bindTexture(...) {
    uint32_t descriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor();

    // Validate descriptor is in bounds
    if (descriptorIndex >= commandBuffer_.getDescriptorHeapSize()) {
        IGL_LOG_ERROR("Failed to bind texture: descriptor heap full\n");
        return; // Graceful failure
    }

    // Proceed with binding...
}
```

**Step 6: Add Diagnostic Telemetry**

```cpp
void CommandBuffer::logDescriptorStats() {
    IGL_LOG_INFO("D3D12 Descriptor Usage:\n");
    IGL_LOG_INFO("  CBV/SRV/UAV: %u / %u (%.1f%%)\n",
                 nextCbvSrvUavDescriptor_, cbvSrvUavHeapSize_,
                 (float)nextCbvSrvUavDescriptor_ / cbvSrvUavHeapSize_ * 100.0f);
    IGL_LOG_INFO("  Samplers: %u / %u (%.1f%%)\n",
                 nextSamplerDescriptor_, samplerHeapSize_,
                 (float)nextSamplerDescriptor_ / samplerHeapSize_ * 100.0f);
}
```

### Recommended Initial Approach

**Phase 1 (Minimum Fix):**
- Add bounds checking with assertions/logging
- Return error or last valid descriptor on overflow
- Reset indices on command buffer reset (ring buffer)

**Phase 2 (Robust Solution):**
- Implement dynamic heap growth
- Add telemetry and usage tracking
- Optimize heap sizing based on workload analysis

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- All descriptor allocation tests must pass
- No device removal errors

**Render Sessions:**
```bash
test_all_sessions.bat
```
Critical sessions for descriptor usage:
- **MRTSession** (multiple render targets = many descriptors)
- **Textured3DCubeSession** (texture sampling)
- **DrawInstancedSession** (instanced rendering)
- **All other sessions** (must pass without regression)

### Validation Steps

1. **Overflow Detection**
   - Enable D3D12 debug layer
   - Run sessions with many resources
   - Verify no "descriptor heap index out of bounds" errors
   - Verify no device removal

2. **Headless Mode Testing** (**CRITICAL**)
   ```bash
   # Run sessions in headless mode (where issue is most common)
   ./MRTSession_d3d12.exe --headless --screenshot-number 0
   ./Textured3DCubeSession_d3d12.exe --headless --screenshot-number 0
   ```
   - Sessions must complete without crashes
   - No device removal errors
   - Descriptor allocation must stay within bounds

3. **Long-Running Sessions**
   - Run sessions for extended duration (60+ seconds)
   - Verify descriptor usage doesn't accumulate indefinitely
   - Check telemetry logs for usage patterns

4. **Stress Testing**
   - Create scene with >1024 unique textures/buffers
   - Verify heap growth or graceful degradation
   - No memory corruption or crashes

5. **Regression Testing**
   - All existing tests pass
   - No performance degradation
   - Visual output unchanged

---

## Success Criteria

- [ ] Bounds checking implemented in `getNextCbvSrvUavDescriptor()`
- [ ] Bounds checking implemented for sampler descriptors (if applicable)
- [ ] Assertions fire when heap overflow detected (debug builds)
- [ ] Graceful error handling prevents crashes (release builds)
- [ ] Ring buffer pattern: descriptors reset on command buffer reset
- [ ] Heap size tracking added to `CommandBuffer` class
- [ ] Telemetry logs descriptor usage statistics
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass in normal mode
- [ ] **All render sessions pass in headless mode** (**CRITICAL**)
- [ ] No device removal errors in any test
- [ ] No validation layer errors about descriptor heaps
- [ ] No memory corruption detected (debug layer, sanitizers)
- [ ] User confirms tests pass, especially headless tests

---

## Dependencies

**Depends On:**
- None (critical bug fix)

**Blocks:**
- TASK_P1_DX12-FIND-05 (Storage Buffer Binding) - Needs safe descriptor allocation
- TASK_P3_DX12-FIND-13 (Multi-Draw Indirect) - Heavy descriptor usage
- All tasks involving resource binding

**Related:**
- TASK_P0_DX12-FIND-01 (Unbounded Descriptor Ranges) - Both affect descriptor management
- TASK_P1_DX12-009 (Upload Ring Buffer) - Similar ring buffer pattern

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts
   - ❌ DO NOT modify render session code
   - ✅ Fix must work with existing test scenarios

2. **User Confirmation Required**
   - ⚠️ Test in headless mode - **MANDATORY**
   - ⚠️ Test with D3D12 debug layer enabled
   - ⚠️ Report descriptor usage statistics
   - ⚠️ Wait for user confirmation

3. **Code Modification Scope**
   - ✅ Modify `CommandBuffer.cpp`, `CommandBuffer.h`
   - ✅ Modify `RenderCommandEncoder.cpp`, `ComputeCommandEncoder.cpp`
   - ✅ Add telemetry and logging
   - ❌ DO NOT change IGL public API
   - ❌ DO NOT modify resource binding semantics

4. **Safety Requirements**
   - Must not crash on descriptor overflow
   - Must provide diagnostic output
   - Must work in headless and windowed modes
   - Must pass validation layer checks

---

**Estimated Timeline:** 2-3 days
**Risk Level:** High (core memory management, affects all rendering)
**Validation Effort:** 4-6 hours (extensive headless testing required)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 02*
