# DX12-COD-005: CBV 64 KB Limit Violation (High Priority)

**Priority:** P1 (High)
**Category:** Descriptors & CBVs
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

CBV descriptors cover entire buffer regardless of requested range, violating D3D12's 64 KB limit for constant buffers.

**In `RenderCommandEncoder.cpp:1615-1640` and `ComputeCommandEncoder.cpp:166-185`:**
```cpp
D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
cbvDesc.BufferLocation = gpuAddress;
cbvDesc.SizeInBytes = alignUp(bufferSize, 256);  // ❌ Can exceed 64 KB

device_->CreateConstantBufferView(&cbvDesc, handle);
```

**D3D12 Requirement:** CBVs must be ≤ 64 KB (65,536 bytes)

---

## Technical Details

### Problem Examples

1. **Buffer size = 128 KB:**
   - Creates CBV with SizeInBytes = 131,072
   - ❌ Violates D3D12 spec → Debug layer error
   
2. **Bind group requests range [1024, 2048]:**
   - Creates CBV covering entire buffer [0, bufferSize]
   - ❌ Ignores requested range

---

## Implementation Guidance

```cpp
// RenderCommandEncoder.cpp
Result RenderCommandEncoder::bindBuffer(...) {
    const auto* buffer = static_cast<Buffer*>(bindGroup->buffers[i].buffer);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = buffer->getGPUAddress() + bindGroup->buffers[i].offset;

    // ✅ Respect requested range
    size_t requestedSize = bindGroup->buffers[i].size > 0
        ? bindGroup->buffers[i].size
        : buffer->getSize();

    // ✅ Enforce 64 KB limit
    if (requestedSize > 65536) {
        return Result{Result::Code::ArgumentOutOfRange,
                     "Constant buffer size exceeds 64 KB limit"};
    }

    cbvDesc.SizeInBytes = static_cast<UINT>(alignUp(requestedSize, 256));

    device_->CreateConstantBufferView(&cbvDesc, handle);
    return Result{};
}
```

**Reference:** [Using Constant Buffers](https://learn.microsoft.com/windows/win32/direct3d12/using-constant-buffers#creating-constant-buffer-views)

---

## Testing Requirements

```cpp
TEST(BindGroupTest, CBV64KBLimit) {
    auto device = createDevice();

    // Create 128 KB buffer
    auto buffer = device->createBuffer({.length = 128 * 1024, ...});

    BindGroupBufferDesc bufferDesc = {};
    bufferDesc.buffer = buffer.get();
    bufferDesc.offset = 0;
    bufferDesc.size = 32 * 1024;  // Request 32 KB

    // Should succeed - respects requested size
    auto bindGroup = device->createBindGroup({...});
    EXPECT_TRUE(bindGroup.isOk());

    // Try to bind >64 KB
    bufferDesc.size = 128 * 1024;
    auto result = device->createBindGroup({...});

    // Should fail with clear error
    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
}
```

---

## Definition of Done

- [ ] Clamp CBVs to requested buffer range
- [ ] Enforce 64 KB maximum
- [ ] Return errors for oversized buffers
- [ ] Debug layer tests pass
- [ ] User confirmation received

---

## Related Issues

- **DX12-COD-006**: Structured buffer stride hard-coded
- **H-006**: Root signature bloat
