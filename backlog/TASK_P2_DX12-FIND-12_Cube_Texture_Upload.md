# TASK_P2_DX12-FIND-12: Implement Cube Texture Upload

**Priority:** P2 - Medium
**Estimated Effort:** 4-6 hours
**Status:** Open
**Issue ID:** DX12-FIND-12 (from Codex Audit)

---

## Problem Statement

`Texture::uploadCube()` is a stub returning `Unimplemented`. Cube map textures cannot be populated on D3D12 backend, blocking environment maps, reflections, and skyboxes.

### Current Behavior
- `uploadCube()` returns `Result::Code::Unimplemented`
- Cube textures cannot be populated with data
- Environment mapping features unavailable

### Expected Behavior
- Upload cube map data (6 faces)
- Support common cube map formats
- Match Vulkan/OpenGL cube map support

---

## Evidence and Code Location

**File:** `src/igl/d3d12/Texture.cpp`

**Search Pattern:**
Find around line 335-340:
```cpp
Result Texture::uploadCube(...) {
    return Result(Result::Code::Unimplemented);
}
```

**Search Keywords:**
- `uploadCube`
- `Unimplemented`
- Cube texture
- Texture array

---

## Impact

**Severity:** Medium - Blocks cube map features
**Functionality:** Cube maps unusable
**Graphics Features:** No environment maps, reflections, skyboxes

**Affected Use Cases:**
- Environment maps
- Reflections
- Skyboxes
- IBL (Image-Based Lighting)

---

## Official References

### Microsoft Documentation
1. **Texture Copy Operations** - [CopyTextureRegion](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations)
2. **Cube Textures** - D3D12_RESOURCE_DIMENSION_TEXTURE2D with array size 6
3. **Subresource Indexing** - Cube faces as array slices

### Cube Map Layout
- 6 faces: +X, -X, +Y, -Y, +Z, -Z
- Each face is 2D texture
- Stored as texture array with 6 slices

---

## Implementation Guidance

### High-Level Approach

1. **Reuse Multi-Subresource Upload**
   - Cube map = texture array with 6 faces
   - Iterate 6 faces, upload each like 2D texture

2. **Subresource Calculation**
   ```cpp
   uint32_t subresource = D3D12CalcSubresource(
       mipLevel,
       arraySlice,  // 0-5 for cube faces
       planeSlice,
       mipLevels,
       arraySize
   );
   ```

3. **Upload Each Face**
   - Similar to existing `Texture::upload()`
   - Copy each face to corresponding array slice

### Detailed Implementation

**uploadCube() Implementation:**
```cpp
Result Texture::uploadCube(const TextureRangeDesc& range,
                           const void* data[6],  // 6 face data pointers
                           Result* outResult) {
    // Validate this is a cube texture
    if (getType() != TextureType::Cube) {
        return Result(Result::Code::ArgumentInvalid, "Not a cube texture");
    }

    // Upload each face
    for (uint32_t face = 0; face < 6; ++face) {
        // Calculate subresource for this face
        uint32_t subresource = D3D12CalcSubresource(
            range.mipLevel,
            face,  // Array slice (cube face)
            0,     // Plane slice
            desc_.numMipLevels,
            6      // Array size for cube
        );

        // Get layout for this subresource
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint64_t rowSize;
        uint64_t totalSize;
        device_->GetCopyableFootprints(
            &resourceDesc_,
            subresource,
            1,  // One subresource
            0,
            &layout,
            nullptr,
            &rowSize,
            &totalSize
        );

        // Create staging buffer for this face
        auto stagingBuffer = createStagingBuffer(totalSize);

        // Copy face data to staging
        void* mapped = nullptr;
        stagingBuffer->Map(0, nullptr, &mapped);

        const uint8_t* srcData = static_cast<const uint8_t*>(data[face]);
        uint8_t* dstData = static_cast<uint8_t*>(mapped);

        // Copy with row pitch consideration
        uint32_t srcRowPitch = range.width * getBytesPerPixel(format_);
        for (uint32_t row = 0; row < range.height; ++row) {
            memcpy(dstData + row * layout.Footprint.RowPitch,
                   srcData + row * srcRowPitch,
                   srcRowPitch);
        }

        stagingBuffer->Unmap(0, nullptr);

        // Record copy command
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = resource_.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = subresource;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = stagingBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layout;

        // Transition, copy, transition back
        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    return Result();
}
```

**Face Order:**
```cpp
// D3D12 cube face order:
// 0: +X
// 1: -X
// 2: +Y
// 3: -Y
// 4: +Z
// 5: -Z
```

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Cube Map Upload Test**
   - Create cube texture
   - Upload 6 faces with distinct data
   - Verify each face uploaded correctly

2. **Cube Map Sampling**
   - Sample cube map in shader
   - Verify correct faces sampled
   - Check environment mapping works

3. **Format Coverage**
   - Test common cube formats (RGBA8, RGBA16F, etc.)

---

## Success Criteria

- [ ] `uploadCube()` implemented
- [ ] All 6 cube faces uploaded correctly
- [ ] Subresource indexing correct for cube faces
- [ ] Cube maps sampled correctly in shaders
- [ ] Common cube formats supported
- [ ] All tests pass
- [ ] User confirms cube maps work

---

## Dependencies

**None** - Can reuse existing upload infrastructure

---

**Estimated Timeline:** 4-6 hours
**Risk Level:** Low (straightforward upload logic)
**Validation Effort:** 2-3 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 12*
