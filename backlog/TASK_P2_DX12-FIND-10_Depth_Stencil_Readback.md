# TASK_P2_DX12-FIND-10: Implement Depth/Stencil Readback Support

**Priority:** P2 - Medium
**Estimated Effort:** 1-2 days
**Status:** Open
**Issue ID:** DX12-FIND-10 (from Codex Audit)

---

## Problem Statement

Depth and stencil attachment readback functions are unimplemented TODOs. Applications cannot read back depth/stencil data from framebuffers, blocking debugging tools, testing, and certain rendering techniques that require depth readback.

### Current Behavior
- `Framebuffer::copyBytesDepthAttachment()` - TODO stub
- `Framebuffer::copyBytesStencilAttachment()` - TODO stub
- Depth/stencil readbacks fail or return empty

### Expected Behavior
- Copy depth attachment to CPU-accessible memory
- Copy stencil attachment to CPU-accessible memory
- Support common depth/stencil formats
- Match color attachment readback functionality

---

## Evidence and Code Location

**File:** `src/igl/d3d12/Framebuffer.cpp`

**Search Pattern:**
Find around line 340-351:
```cpp
void Framebuffer::copyBytesDepthAttachment(...) {
    // TODO
}

void Framebuffer::copyBytesStencilAttachment(...) {
    // TODO
}
```

**Search Keywords:**
- `copyBytesDepthAttachment`
- `copyBytesStencilAttachment`
- TODO in Framebuffer

---

## Impact

**Severity:** Medium - Blocks specific use cases
**Functionality:** Depth/stencil readback unavailable
**Tools:** Blocks debugging and testing utilities

**Affected Use Cases:**
- Depth buffer visualization
- Testing/validation tools
- Shadow map debugging
- Certain post-processing techniques

---

## Official References

### Microsoft Documentation
1. **Texture Copy Operations** - [CopyTextureRegion](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion)
2. **Readback Heaps** - D3D12_HEAP_TYPE_READBACK
3. **Depth Format Mapping** - Handle typeless formats

---

## Implementation Guidance

### High-Level Approach

1. **Create Readback Buffer**
   - Allocate staging texture in readback heap
   - Match depth/stencil format

2. **Transition Resource States**
   - Depth/stencil → `D3D12_RESOURCE_STATE_COPY_SOURCE`

3. **Copy Texture Region**
   - Use `CopyTextureRegion()` to copy depth data

4. **Map and Read**
   - Map readback buffer
   - Copy to CPU memory
   - Unmap

### Format Handling

Depth/stencil formats require special handling:
- `DXGI_FORMAT_D24_UNORM_S8_UINT` - 24-bit depth + 8-bit stencil
- `DXGI_FORMAT_D32_FLOAT` - 32-bit depth
- May need to extract depth/stencil planes separately

### Example Implementation

```cpp
void Framebuffer::copyBytesDepthAttachment(uint8_t* dst, ...) {
    // Get depth texture
    auto* depthTexture = depthAttachment_->getD3D12Resource();

    // Create readback buffer
    auto readbackBuffer = createReadbackBuffer(width, height, depthFormat);

    // Transition depth texture to copy source
    transitionResource(depthTexture,
                      D3D12_RESOURCE_STATE_DEPTH_WRITE,
                      D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Copy depth data
    commandList->CopyTextureRegion(
        &readbackDst,
        0, 0, 0,
        &depthSrc,
        nullptr
    );

    // Transition back
    transitionResource(depthTexture,
                      D3D12_RESOURCE_STATE_COPY_SOURCE,
                      D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // Execute and wait
    executeAndWait();

    // Map and copy
    void* mappedData = nullptr;
    readbackBuffer->Map(0, nullptr, &mappedData);
    memcpy(dst, mappedData, dataSize);
    readbackBuffer->Unmap(0, nullptr);
}
```

### Stencil Extraction

For formats with stencil (D24S8):
- Use `D3D12_TEX2D_DSV::PlaneSlice` to access stencil plane
- Copy stencil plane separately

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Depth Readback Test**
   - Render to depth buffer
   - Read back depth values
   - Verify depth data is correct

2. **Stencil Readback Test**
   - Render with stencil operations
   - Read back stencil values
   - Verify stencil data matches expected

3. **Format Coverage**
   - Test common depth formats (D32, D24S8, D16)
   - Verify all formats work

---

## Success Criteria

- [ ] `copyBytesDepthAttachment()` implemented
- [ ] `copyBytesStencilAttachment()` implemented
- [ ] Common depth/stencil formats supported
- [ ] Resource state transitions correct
- [ ] Depth readback returns correct data
- [ ] Stencil readback returns correct data
- [ ] All tests pass
- [ ] User confirms readback works

---

## Dependencies

**Related:**
- Mirror color attachment readback logic (already working)

---

**Estimated Timeline:** 1-2 days
**Risk Level:** Low-Medium
**Validation Effort:** 3-4 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 10*
