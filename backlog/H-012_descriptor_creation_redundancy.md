# H-012: Descriptor Creation Redundancy (High Priority)

**Priority:** P1 (High)
**Category:** Performance & Scalability
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

The D3D12 backend recreates identical texture descriptors every frame instead of caching. This causes:

1. **Performance waste** - 100-200μs per frame creating duplicate SRV descriptors
2. **Descriptor heap bloat** - Same texture bound multiple times creates multiple descriptors
3. **Cache misses** - No texture→descriptor mapping, linear search every bind
4. **Scalability issue** - 100 unique textures × 60 FPS = 6000 redundant descriptor creations/second

---

## Technical Details

### Current Problem

**In `RenderCommandEncoder.cpp`:**
```cpp
void RenderCommandEncoder::bindTexture(ITexture* texture, uint32_t index) {
    // ❌ ALWAYS creates new SRV descriptor, even if texture already has one
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    commandBuffer_.getNextCbvSrvUavDescriptor(cpuHandle, gpuHandle);

    // Create SRV
    device_->CreateShaderResourceView(
        d3d12Texture->getResource(),
        &srvDesc,
        cpuHandle);  // ❌ Same texture, new descriptor EVERY frame

    cmdList_->SetGraphicsRootDescriptorTable(index, gpuHandle);
}

// No cache:  100 textures × 60 FPS = 6000 CreateSRV calls/second
// With cache: 100 textures once = 100 CreateSRV calls total
```

### Correct Pattern (Descriptor Caching)

```cpp
// ✅ Cache descriptor per texture
class Texture {
private:
    std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> cachedSRVDescriptor_;
};

D3D12_GPU_DESCRIPTOR_HANDLE Texture::getSRVDescriptor() {
    if (!cachedSRVDescriptor_) {
        // Create once and cache
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

        device_->allocatePersistentDescriptor(cpuHandle, gpuHandle);

        device_->CreateShaderResourceView(resource_.Get(), &srvDesc_, cpuHandle);

        cachedSRVDescriptor_ = gpuHandle;
    }

    return *cachedSRVDescriptor_;
}
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Texture.h`**
   - Add `cachedSRVDescriptor_` member
   - Add `getSRVDescriptor()` method

2. **`src/igl/d3d12/RenderCommandEncoder.cpp`**
   - Use cached descriptor instead of creating new one
   - Only create descriptor on first use

3. **`src/igl/d3d12/DescriptorHeapManager.cpp`**
   - Add persistent descriptor allocation (separate from per-frame heap)

---

## Official References

- [MiniEngine DynamicDescriptorHeap.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicDescriptorHeap.cpp)
  - Lines 100-150: Descriptor caching pattern

---

## Implementation Guidance

```cpp
// Texture.h
class Texture {
public:
    D3D12_GPU_DESCRIPTOR_HANDLE getSRVDescriptor();
    D3D12_GPU_DESCRIPTOR_HANDLE getUAVDescriptor();

private:
    std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> cachedSRV_;
    std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> cachedUAV_;
};

// Texture.cpp
D3D12_GPU_DESCRIPTOR_HANDLE Texture::getSRVDescriptor() {
    if (!cachedSRV_) {
        auto handle = device_->allocatePersistentSRVDescriptor();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        // ... fill SRV desc ...

        device_->CreateShaderResourceView(
            resource_.Get(),
            &srvDesc,
            handle.cpuHandle);

        cachedSRV_ = handle.gpuHandle;
    }

    return *cachedSRV_;
}

// RenderCommandEncoder.cpp
void RenderCommandEncoder::bindTexture(ITexture* texture, uint32_t index) {
    auto* d3d12Texture = static_cast<Texture*>(texture);

    // ✅ Use cached descriptor (created once per texture)
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = d3d12Texture->getSRVDescriptor();

    cmdList_->SetGraphicsRootDescriptorTable(index, gpuHandle);

    // Performance: ~1μs (was 100-200μs with CreateSRV)
}
```

---

## Testing Requirements

```cpp
TEST(TextureTest, DescriptorCaching) {
    auto device = createDevice();
    auto texture = device->createTexture({...});

    // First access creates descriptor
    auto handle1 = texture->getSRVDescriptor();

    // Second access returns cached descriptor
    auto handle2 = texture->getSRVDescriptor();

    EXPECT_EQ(handle1.ptr, handle2.ptr);  // Same descriptor
}
```

---

## Definition of Done

- [ ] Texture descriptor caching implemented
- [ ] Persistent descriptor heap added
- [ ] Performance measured (100-200μs → <5μs per bind)
- [ ] All render sessions work
- [ ] User confirmation received

---

## Related Issues

- **H-006**: Root signature bloat (descriptor table usage)
- **H-007**: No descriptor bounds check (caching reduces allocations)
