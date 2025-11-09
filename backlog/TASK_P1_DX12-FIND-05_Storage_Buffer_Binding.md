# TASK_P1_DX12-FIND-05: Implement Bind Group Storage Buffer Support

**Priority:** P1 - High
**Estimated Effort:** 2-3 days
**Status:** Open
**Issue ID:** DX12-FIND-05 (from Codex Audit)

---

## Problem Statement

Storage buffers accessed through bind groups are not implemented. The code logs "SRV/UAV table binding pending RS update" but never creates or binds the descriptors, making descriptor-based storage buffers unusable on D3D12.

### Current Behavior
- `RenderCommandEncoder::bindBindGroup()` logs: "SRV/UAV table binding pending"
- Storage buffer descriptors never allocated
- No SRV/UAV creation for storage buffers
- Shaders read/write undefined memory

### Expected Behavior
- Storage buffer SRV/UAV descriptors created
- Descriptors allocated in per-frame heap
- Bound via `SetGraphicsRootDescriptorTable()`
- Shaders can read/write storage buffers

---

## Evidence and Code Location

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Search Pattern:**
1. Find `RenderCommandEncoder::bindBindGroup()` function
2. Look for log: "SRV/UAV table binding pending"
3. Around line 1518-1531 (from Codex reference)
4. Find storage buffer handling logic

**Search Keywords:**
- `bindBindGroup`
- "pending RS update"
- Storage buffer
- UAV binding
- SRV binding

---

## Impact

**Severity:** High - Feature not working
**Functionality:** Storage buffers via bind groups unusable
**API Coverage:** Breaks IGL contract

**Affected Code Paths:**
- Render passes using storage buffers in bind groups
- Read/write buffers in shaders
- Structured buffer scenarios

---

## Official References

### Microsoft Documentation

1. **UAV Descriptors** - [Unordered Access Views](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uavs)
2. **CreateUnorderedAccessView** - [ID3D12Device Method](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview)
3. **CreateShaderResourceView** - For read-only storage buffers

### Cross-Reference
- **Vulkan Backend:** `src/igl/vulkan/ResourcesBinder.cpp:52-185` - Implements storage buffer binding

---

## Implementation Guidance

### High-Level Approach

1. **Determine Read/Write Access**
   - Read-only storage buffers → SRV
   - Read-write storage buffers → UAV

2. **Allocate Descriptors**
   - Get descriptor(s) from per-frame heap
   - Allocate contiguously for multiple storage buffers

3. **Create SRV/UAV Descriptors**
   - Call `CreateShaderResourceView()` or `CreateUnorderedAccessView()`
   - Configure for structured/raw buffers as needed

4. **Bind Descriptor Table**
   - `SetGraphicsRootDescriptorTable()` with appropriate root parameter

### Detailed Steps

**Step 1: Identify Root Signature Layout**
- Find SRV/UAV descriptor table in root signature
- Determine root parameter index
- Check descriptor range configuration

**Step 2: Implement Storage Buffer Binding**

In `RenderCommandEncoder::bindBindGroup()`:

```cpp
void RenderCommandEncoder::bindBindGroup(...) {
    // ... existing texture/sampler bindings ...

    // Bind storage buffers
    for (auto& storageBuffer : bindGroup.storageBuffers) {
        // Allocate descriptor
        uint32_t descriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor();
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = getCPUHandle(descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = getGPUHandle(descriptorIndex);

        if (storageBuffer.isReadWrite()) {
            // Create UAV for read-write access
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = storageBuffer.elementCount();
            uavDesc.Buffer.StructureByteStride = storageBuffer.stride();
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

            device_->CreateUnorderedAccessView(
                storageBuffer.d3d12Resource(),
                nullptr,  // No counter resource
                &uavDesc,
                cpuHandle
            );
        } else {
            // Create SRV for read-only access
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = storageBuffer.elementCount();
            srvDesc.Buffer.StructureByteStride = storageBuffer.stride();
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            device_->CreateShaderResourceView(
                storageBuffer.d3d12Resource(),
                &srvDesc,
                cpuHandle
            );
        }

        // Bind descriptor table
        commandList_->SetGraphicsRootDescriptorTable(
            rootParameterIndexForStorage,
            gpuHandle
        );
    }
}
```

**Step 3: Handle Raw Buffers vs Structured Buffers**
- Structured: Set `StructureByteStride`
- Raw (ByteAddressBuffer): Set `Flags = D3D12_BUFFER_UAV_FLAG_RAW`

**Step 4: Resource State Transitions**
- UAV: Transition to `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`
- SRV: Transition to `D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE` or `D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE`

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

**Important:**
- Any session using storage buffers
- Bind group tests

### Validation Steps

1. **Storage Buffer Functionality**
   - Test read-only storage buffers (SRV)
   - Test read-write storage buffers (UAV)
   - Verify correct data read/written

2. **Debug Layer**
   - No errors about unbound resources
   - No errors about invalid descriptors

---

## Success Criteria

- [ ] Storage buffer descriptors created (SRV/UAV)
- [ ] Descriptors bound via `SetGraphicsRootDescriptorTable()`
- [ ] Resource states transitioned correctly
- [ ] Both read-only and read-write buffers work
- [ ] All tests pass
- [ ] No debug layer errors
- [ ] User confirms storage buffer functionality

---

## Dependencies

**Depends On:**
- TASK_P0_DX12-FIND-02 (Descriptor Heap Overflow) - **CRITICAL**: Need safe descriptor allocation first

**Related:**
- TASK_P1_DX12-FIND-04 (Compute CBV) - Similar descriptor binding pattern

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **User Confirmation Required:** ⚠️ Test storage buffer scenarios
3. **Dependencies:** Ensure descriptor heap fixes completed first

---

**Estimated Timeline:** 2-3 days
**Risk Level:** Medium-High (depends on descriptor heap fix)
**Validation Effort:** 3-4 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 05*
