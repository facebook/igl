# TASK_P3_DX12-FIND-13: Implement Multi-Draw Indirect Support

**Priority:** P3 - Low
**Estimated Effort:** 1-2 weeks
**Status:** Open
**Issue ID:** DX12-FIND-13 (from Codex Audit)

---

## Problem Statement

`multiDrawIndirect()` and `multiDrawIndexedIndirect()` are empty stubs. IGL's multi-draw indirect APIs are non-functional on D3D12, preventing efficient multi-object rendering and GPU-driven rendering techniques.

### Current Behavior
- `RenderCommandEncoder::multiDrawIndirect()` - Empty stub
- `RenderCommandEncoder::multiDrawIndexedIndirect()` - Empty stub
- Multi-draw APIs advertised by IGL don't work on D3D12

### Expected Behavior
- Implement using D3D12 `ExecuteIndirect()` command
- Support GPU-driven rendering
- Match Vulkan `vkCmdDrawIndirect()` functionality

---

## Evidence and Code Location

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Search Pattern:**
Find around line 1158-1163:
```cpp
void RenderCommandEncoder::multiDrawIndirect(...) {
    // Empty stub
}

void RenderCommandEncoder::multiDrawIndexedIndirect(...) {
    // Empty stub
}
```

**Search Keywords:**
- `multiDrawIndirect`
- `multiDrawIndexedIndirect`
- Empty stubs

---

## Impact

**Severity:** Low - Advanced feature
**Functionality:** Multi-draw indirect unavailable
**Performance:** Cannot use GPU-driven rendering optimizations

**Affected Use Cases:**
- GPU culling
- Instanced multi-object rendering
- Draw call batching
- Advanced rendering techniques

---

## Official References

### Microsoft Documentation
1. **ExecuteIndirect** - [ID3D12GraphicsCommandList::ExecuteIndirect](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-executeindirect)
2. **Command Signature** - [D3D12_COMMAND_SIGNATURE_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_command_signature_desc)

### DirectX-Graphics-Samples
- **D3D12ExecuteIndirect** - Complete sample implementation
  - [GitHub Link](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12ExecuteIndirect)

---

## Implementation Guidance

### High-Level Approach

1. **Create Command Signature**
   ```cpp
   D3D12_COMMAND_SIGNATURE_DESC sigDesc = {};
   sigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
   sigDesc.NumArgumentDescs = 1;
   sigDesc.pArgumentDescs = &argDesc;  // DRAW or DRAW_INDEXED

   device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&signature));
   ```

2. **Implement multiDrawIndirect()**
   ```cpp
   void multiDrawIndirect(Buffer& argsBuffer, uint32_t count, ...) {
       commandList->ExecuteIndirect(
           drawCommandSignature_,
           count,
           argsBuffer.getD3D12Resource(),
           argsOffset,
           nullptr,  // No count buffer
           0
       );
   }
   ```

3. **Handle Indexed Variant**
   - Use `D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED`
   - Create separate command signature

### Detailed Steps

**Step 1: Create Command Signatures in Device**

```cpp
// In Device initialization:
void Device::createIndirectCommandSignatures() {
    // Draw signature
    D3D12_INDIRECT_ARGUMENT_DESC drawArg = {};
    drawArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC drawSigDesc = {};
    drawSigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);  // 16 bytes
    drawSigDesc.NumArgumentDescs = 1;
    drawSigDesc.pArgumentDescs = &drawArg;

    device_->CreateCommandSignature(&drawSigDesc, nullptr,
                                    IID_PPV_ARGS(&drawIndirectSignature_));

    // Draw indexed signature
    D3D12_INDIRECT_ARGUMENT_DESC drawIndexedArg = {};
    drawIndexedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC drawIndexedSigDesc = {};
    drawIndexedSigDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);  // 20 bytes
    drawIndexedSigDesc.NumArgumentDescs = 1;
    drawIndexedSigDesc.pArgumentDescs = &drawIndexedArg;

    device_->CreateCommandSignature(&drawIndexedSigDesc, nullptr,
                                    IID_PPV_ARGS(&drawIndexedIndirectSignature_));
}
```

**Step 2: Implement multiDrawIndirect()**

```cpp
void RenderCommandEncoder::multiDrawIndirect(
    const Buffer& indirectBuffer,
    size_t indirectBufferOffset,
    uint32_t drawCount,
    const Buffer* drawCountBuffer,
    size_t drawCountBufferOffset
) {
    auto* d3dBuffer = static_cast<const D3D12Buffer&>(indirectBuffer).getResource();

    if (drawCountBuffer) {
        // With count buffer (max count specified)
        auto* d3dCountBuffer = static_cast<const D3D12Buffer*>(drawCountBuffer)->getResource();
        commandList_->ExecuteIndirect(
            device_->getDrawIndirectSignature(),
            drawCount,  // Max draw count
            d3dBuffer,
            indirectBufferOffset,
            d3dCountBuffer,
            drawCountBufferOffset
        );
    } else {
        // Without count buffer (exact count)
        commandList_->ExecuteIndirect(
            device_->getDrawIndirectSignature(),
            drawCount,
            d3dBuffer,
            indirectBufferOffset,
            nullptr,
            0
        );
    }
}
```

**Step 3: Implement multiDrawIndexedIndirect()**

Similar pattern using `drawIndexedIndirectSignature_`.

**Step 4: Argument Buffer Layout**

Ensure indirect buffer contains correct data:
```cpp
// D3D12_DRAW_ARGUMENTS
struct DrawArgs {
    uint32_t vertexCountPerInstance;
    uint32_t instanceCount;
    uint32_t startVertexLocation;
    uint32_t startInstanceLocation;
};

// D3D12_DRAW_INDEXED_ARGUMENTS
struct DrawIndexedArgs {
    uint32_t indexCountPerInstance;
    uint32_t instanceCount;
    uint32_t startIndexLocation;
    int32_t baseVertexLocation;
    uint32_t startInstanceLocation;
};
```

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Basic Multi-Draw Test**
   - Create indirect buffer with multiple draw commands
   - Execute multi-draw
   - Verify all draws executed

2. **With Count Buffer**
   - Test GPU-driven draw count
   - Verify count buffer respected

3. **Indexed Variant**
   - Test indexed multi-draw
   - Verify index buffers used correctly

---

## Success Criteria

- [ ] Command signatures created
- [ ] `multiDrawIndirect()` implemented
- [ ] `multiDrawIndexedIndirect()` implemented
- [ ] `ExecuteIndirect()` called correctly
- [ ] Optional count buffer supported
- [ ] All tests pass
- [ ] Multi-draw API functional
- [ ] User confirms multi-draw works

---

## Dependencies

**Depends On:**
- TASK_P0_DX12-FIND-02 (Descriptor Heap Overflow) - Safe descriptor usage required

**Related:**
- Advanced rendering techniques
- GPU-driven rendering

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **Complexity:** Advanced D3D12 feature, requires careful implementation
3. **Testing:** Requires creating indirect argument buffers

---

**Estimated Timeline:** 1-2 weeks
**Risk Level:** Medium-High (complex feature)
**Validation Effort:** 1 week

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 13*
