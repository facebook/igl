# DX12-COD-006: Structured Buffer Stride Hard-Coded (High Priority)

**Priority:** P1 (High)
**Category:** Descriptors & SRV/UAVs
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

Structured SRVs/UAVs hard-code `StructureByteStride = 4` and compute `NumElements = bufferSize / 4`, mis-addressing any buffer with different element size.

**In `RenderCommandEncoder.cpp:1672-1715` and `ComputeCommandEncoder.cpp:343-379`:**
```cpp
D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format = DXGI_FORMAT_UNKNOWN;
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Buffer.FirstElement = 0;
srvDesc.Buffer.NumElements = bufferSize / 4;  // ❌ Assumes 4-byte elements
srvDesc.Buffer.StructureByteStride = 4;       // ❌ Hard-coded
srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
```

**Problem:** All structured buffers treated as `uint` arrays, regardless of actual stride.

---

## Technical Details

### Impact Examples

1. **Struct with 16-byte stride:**
   ```hlsl
   struct Particle {
       float3 position;  // 12 bytes
       float mass;       // 4 bytes
   };  // Total: 16 bytes
   
   StructuredBuffer<Particle> particles;
   ```
   - Correct: stride=16, count=bufferSize/16
   - Actual: stride=4, count=bufferSize/4
   - Result: Reading particles[1] accesses wrong memory

2. **Struct with 32-byte stride:**
   ```hlsl
   struct Instance {
       float4x4 transform;  // 64 bytes
   };
   ```
   - Reads misaligned data, causes visual corruption

---

## Implementation Guidance

### Step 1: Store Stride in BufferDesc

```cpp
// Include BufferDesc.h - add stride field
struct BufferDesc {
    // ... existing fields ...
    size_t elementStride = 0;  // ✅ Add stride (0 = unstructured)
};
```

### Step 2: Propagate Stride Through Buffer Creation

```cpp
// Buffer.h
class Buffer {
public:
    size_t getElementStride() const { return elementStride_; }

private:
    size_t elementStride_ = 0;
};

// Device.cpp - createBuffer
std::shared_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc) {
    auto buffer = std::make_shared<Buffer>(...);
    buffer->setElementStride(desc.elementStride);
    return buffer;
}
```

### Step 3: Use Correct Stride in Descriptor Creation

```cpp
// RenderCommandEncoder.cpp
void RenderCommandEncoder::bindBuffer(...) {
    const auto* buffer = static_cast<const Buffer*>(bindGroup->buffers[i].buffer);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // ✅ Use actual element stride
    size_t stride = buffer->getElementStride();
    if (stride == 0) {
        // Unstructured buffer - use raw bytes
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = buffer->getSize() / 4;  // 4-byte units
        srvDesc.Buffer.StructureByteStride = 0;  // Raw buffer
    } else {
        // Structured buffer - use element stride
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(buffer->getSize() / stride);
        srvDesc.Buffer.StructureByteStride = static_cast<UINT>(stride);
    }

    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    device_->CreateShaderResourceView(
        buffer->getResource(),
        &srvDesc,
        handle
    );
}

// ComputeCommandEncoder.cpp - same pattern for UAVs
void ComputeCommandEncoder::bindBuffer(...) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

    size_t stride = buffer->getElementStride();
    if (stride > 0) {
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = static_cast<UINT>(buffer->getSize() / stride);
        uavDesc.Buffer.StructureByteStride = static_cast<UINT>(stride);
    } else {
        // Raw UAV
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = buffer->getSize() / 4;
        uavDesc.Buffer.StructureByteStride = 0;
    }

    device_->CreateUnorderedAccessView(
        buffer->getResource(),
        nullptr,
        &uavDesc,
        handle
    );
}
```

**Reference:** [Structured Buffers](https://learn.microsoft.com/windows/win32/direct3d12/structured-buffers)

---

## Testing Requirements

```cpp
TEST(BufferTest, StructuredBufferStride) {
    auto device = createDevice();

    // Create structured buffer with 16-byte elements
    BufferDesc desc = {};
    desc.length = 1024;  // 64 elements of 16 bytes
    desc.elementStride = 16;
    desc.usage = BufferUsageBits::Storage;

    auto buffer = device->createBuffer(desc);
    EXPECT_EQ(buffer->getElementStride(), 16);

    // Bind to compute shader
    auto bindGroup = device->createBindGroup({
        .buffers = {{.buffer = buffer.get()}}
    });

    // Verify SRV created with correct stride
    // (would need access to internal descriptor to verify)
}

TEST(BufferTest, RawBuffer) {
    auto device = createDevice();

    // Create raw buffer (no stride)
    BufferDesc desc = {};
    desc.length = 1024;
    desc.elementStride = 0;  // Raw buffer

    auto buffer = device->createBuffer(desc);
    EXPECT_EQ(buffer->getElementStride(), 0);
}
```

---

## Definition of Done

- [ ] Add `elementStride` field to BufferDesc
- [ ] Store stride in Buffer class
- [ ] Use correct stride in SRV/UAV descriptor creation
- [ ] Handle stride=0 as raw buffer
- [ ] Unit tests verify correct element addressing
- [ ] Debug layer validation passes
- [ ] User confirmation received

---

## Related Issues

- **DX12-COD-005**: CBV 64 KB limit violation
- **H-006**: Root signature bloat
