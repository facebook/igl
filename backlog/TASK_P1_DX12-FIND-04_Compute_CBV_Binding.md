# TASK_P1_DX12-FIND-04: Implement Compute Constant Buffer Descriptor Binding

**Priority:** P1 - High
**Estimated Effort:** 1-2 days
**Status:** Open
**Issue ID:** DX12-FIND-04 (from Codex Audit)

---

## Problem Statement

Compute shader constant buffer views (CBVs) are never bound to the descriptor table (root parameter 3). The code logs "using direct root CBV for now" but never actually populates the CBV descriptors, causing compute shaders to read undefined data for constant buffers.

### Current Behavior
- Compute root signature includes CBV descriptor table (root parameter 3)
- `ComputeCommandEncoder::bindBindGroup()` logs: "CBV table binding ... using direct root CBV for now"
- Descriptors never created or bound
- Compute shaders read garbage data for constants

### Expected Behavior
- CBV descriptors allocated in per-frame descriptor heap
- `CreateConstantBufferView()` called for each constant buffer
- GPU descriptor handle bound via `SetComputeRootDescriptorTable(3, ...)`
- Compute shaders receive valid constant data

---

## Evidence and Code Location

**File:** `src/igl/d3d12/ComputeCommandEncoder.cpp`

**Search Pattern:**
1. Find function `ComputeCommandEncoder::bindBindGroup()` or similar
2. Look for log message: "using direct root CBV for now"
3. Around line 155-170 (from Codex reference)
4. Find root parameter 3 binding logic (or lack thereof)

**Search Keywords:**
- `SetComputeRootDescriptorTable`
- `CreateConstantBufferView`
- Compute CBV
- Root parameter 3
- "using direct root CBV"

---

## Impact

**Severity:** High - Compute shaders unusable
**Functionality:** Breaks compute pipeline functionality
**Data Corruption:** Shaders read undefined memory

**Affected Code Paths:**
- All compute shader executions using constant buffers
- Compute-based rendering techniques
- Any compute dispatch with uniforms

---

## Official References

### Microsoft Documentation

1. **Descriptor Tables** - [Using Root Signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#descriptor-tables)
2. **CreateConstantBufferView** - [ID3D12Device::CreateConstantBufferView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createconstantbufferview)
   ```cpp
   void CreateConstantBufferView(
     const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
     D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor
   );
   ```
3. **SetComputeRootDescriptorTable** - [ID3D12GraphicsCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setcomputerootdescriptortable)

### DirectX-Graphics-Samples
- **D3D12HelloConstBuffers** - Basic CBV binding
- **D3D12nBodyGravity** - Compute shader with constants

---

## Implementation Guidance

### High-Level Approach

1. **Allocate CBV Descriptors**
   - Get descriptor from per-frame heap
   - Similar to texture SRV allocation

2. **Create Constant Buffer Views**
   - Call `CreateConstantBufferView()` with buffer GPU address
   - Ensure buffer size is 256-byte aligned

3. **Bind Descriptor Table**
   - Call `SetComputeRootDescriptorTable(3, gpuHandle)`
   - Pass GPU descriptor handle (not CPU)

### Detailed Steps

**Step 1: Identify Root Parameter Index**
- Find compute root signature definition
- Verify root parameter 3 is CBV table
- Check descriptor range configuration

**Step 2: Modify bindBindGroup() in ComputeCommandEncoder**

Before (BROKEN):
```cpp
void ComputeCommandEncoder::bindBindGroup(...) {
    // ... other bindings ...

    // CBV binding
    IGL_LOG_INFO("CBV table binding ... using direct root CBV for now");
    // ← Missing: Create CBV descriptors and bind
}
```

After (WORKING):
```cpp
void ComputeCommandEncoder::bindBindGroup(...) {
    // ... other bindings ...

    // Bind constant buffers to CBV table (root parameter 3)
    if (bindGroup has constant buffers) {
        // Allocate descriptor(s) from per-frame heap
        uint32_t cbvDescriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor();
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = getCPUDescriptorHandle(cbvDescriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = getGPUDescriptorHandle(cbvDescriptorIndex);

        // Create constant buffer view
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = alignTo256(constantBuffer->size());  // Must be 256-byte aligned
        device_->CreateConstantBufferView(&cbvDesc, cpuHandle);

        // Bind descriptor table
        commandList_->SetComputeRootDescriptorTable(3, gpuHandle);
    }
}
```

**Step 3: Handle Multiple Constant Buffers**
- If multiple CBVs needed, allocate contiguous descriptors
- Create CBVs for each buffer
- Bind base descriptor handle

**Step 4: Ensure 256-Byte Alignment**
```cpp
inline uint32_t alignTo256(uint32_t size) {
    return (size + 255) & ~255;
}
```

**Step 5: Verify Root Signature Match**
- Ensure root signature descriptor range matches binding logic
- Verify compute shaders expect CBV at correct register (e.g., `b0`)

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

**Critical Sessions:**
- ComputeSession (if exists)
- Any session using compute shaders
- All sessions (regression)

### Validation Steps

1. **Compute Functionality Test**
   - Run compute-based session
   - Verify compute shaders execute correctly
   - Check output matches expected results

2. **Constant Data Validation**
   - Add logging to verify CBV GPU address
   - Verify descriptors created successfully
   - Check shader receives correct constant values

3. **Debug Layer Validation**
   - Enable D3D12 debug layer
   - No errors about unbound descriptors
   - No warnings about invalid CBVs

---

## Success Criteria

- [ ] CBV descriptors allocated from per-frame heap
- [ ] `CreateConstantBufferView()` called for constant buffers
- [ ] CBVs bound via `SetComputeRootDescriptorTable(3, ...)`
- [ ] Buffer sizes 256-byte aligned
- [ ] Compute shaders receive correct constant data
- [ ] All unit tests pass
- [ ] All render sessions pass (especially compute)
- [ ] No debug layer errors about CBV binding
- [ ] User confirms compute functionality works

---

## Dependencies

**Depends On:**
- TASK_P0_DX12-FIND-02 (Descriptor Heap Overflow) - Need safe descriptor allocation

**Blocks:**
- Full compute pipeline functionality
- Compute-based rendering features

**Related:**
- TASK_P1_DX12-FIND-05 (Storage Buffers) - Similar descriptor binding

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **User Confirmation Required:** ⚠️ Test compute sessions
3. **Code Scope:** ✅ Modify `ComputeCommandEncoder.cpp` and related
4. **Validation:** Must pass debug layer checks

---

**Estimated Timeline:** 1-2 days
**Risk Level:** Medium (compute functionality critical)
**Validation Effort:** 3-4 hours (compute-specific testing)

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 04*
