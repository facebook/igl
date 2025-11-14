# H-005: Missing UAV Barrier API After Compute (High Priority)

**Priority:** P1 (High)
**Category:** Resources & Memory Lifetime
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

The D3D12 backend does not provide an API to insert UAV (Unordered Access View) barriers after compute shader dispatches. This causes:

1. **Undefined behavior** - GPU reads stale data from UAV if no barrier inserted
2. **Race conditions** - Overlapping compute dispatches write to same UAV without synchronization
3. **Visual corruption** - Particle systems, post-processing show flickering/artifacts
4. **Write-after-write hazard** - Multiple dispatches modify same resource without ordering

This is a **high-priority correctness issue** - compute shaders require explicit UAV barriers for correctness.

---

## Technical Details

### Current Problem

**In `RenderCommandEncoder.cpp`:**
```cpp
// ❌ NO API TO INSERT UAV BARRIERS
// After compute dispatch, no way to synchronize UAV access

void RenderCommandEncoder::dispatchCompute(...) {
    cmdList_->Dispatch(threadGroupsX, threadGroupsY, threadGroupsZ);

    // ❌ MISSING: Global UAV barrier
    // Multiple dispatches in same encoder will race on UAV access
}
```

**Example failure scenario:**
```cpp
// Compute pass 1: Write particles to UAV buffer
encoder->bindComputePipeline(particleUpdatePipeline);
encoder->bindBuffer(particleBuffer, 0);  // UAV
encoder->dispatchCompute(1024, 1, 1);

// ❌ NO BARRIER HERE - GPU may not see writes from previous dispatch

// Compute pass 2: Read particles from same buffer
encoder->bindComputePipeline(particleRenderPipeline);
encoder->bindBuffer(particleBuffer, 0);  // SRV (reading same data)
encoder->dispatchCompute(1024, 1, 1);  // ❌ RACE: May read stale data
```

### Microsoft D3D12 UAV Barrier Requirements

From [Resource Barriers documentation](https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12):

> **UAV barriers are required when:**
> 1. A UAV is written by one dispatch and read/written by another
> 2. Multiple overlapping dispatches write to the same UAV
> 3. Read-after-write dependencies exist within a command list
>
> **Use `D3D12_RESOURCE_BARRIER_TYPE_UAV`** to ensure visibility and ordering.

### Correct Pattern (from Microsoft samples)

```cpp
// ✅ CORRECT: Insert UAV barrier between dependent dispatches
void ComputeCommandEncoder::dispatchCompute(...) {
    cmdList_->Dispatch(...);

    // Insert global UAV barrier (flushes all UAV caches)
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = nullptr;  // nullptr = global UAV barrier

    cmdList_->ResourceBarrier(1, &barrier);
}

// ✅ BETTER: Specific UAV barrier (only flush specific resource)
void ComputeCommandEncoder::barrierUAV(IBuffer* uavBuffer) {
    auto* d3d12Buffer = static_cast<Buffer*>(uavBuffer);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = d3d12Buffer->getD3D12Resource();

    cmdList_->ResourceBarrier(1, &barrier);
}
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/ICommandEncoder.h`** (cross-platform API)
   - Add `virtual void barrierUAV(ITexture* texture) = 0;`
   - Add `virtual void barrierUAV(IBuffer* buffer) = 0;`
   - Add `virtual void barrierAllUAVs() = 0;` (global barrier)

2. **`src/igl/d3d12/RenderCommandEncoder.h`**
   - Implement UAV barrier API
   - Add `void barrierUAV(ITexture* texture) override;`

3. **`src/igl/d3d12/ComputeCommandEncoder.h`**
   - Implement UAV barrier API
   - Add automatic barrier after `dispatchCompute()` (optional flag)

4. **`src/igl/d3d12/ComputeCommandEncoder.cpp`**
   - Implement `barrierUAV()` methods
   - Optionally insert automatic barriers between dispatches

### Key Identifiers

- **Barrier type:** `D3D12_RESOURCE_BARRIER_TYPE_UAV`
- **Global barrier:** `barrier.UAV.pResource = nullptr`
- **Specific barrier:** `barrier.UAV.pResource = resource`

---

## Official References

### Microsoft Documentation

- [Resource Barriers](https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
  - Section: "UAV Barriers"
  - Key rule: "Insert UAV barriers for write-after-write and read-after-write"

- [D3D12_RESOURCE_BARRIER_TYPE_UAV](https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_barrier_type)
  - Documentation for UAV barrier type

- [Compute Sample (N-Body)](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12nBodyGravity/src/D3D12nBodyGravity.cpp)
  - Lines 400-420: UAV barriers between compute dispatches
  - Shows global and specific UAV barrier patterns

---

## Implementation Guidance

### Step 1: Add Cross-Platform API

```cpp
// ICommandEncoder.h (cross-platform interface)
class ICommandEncoder {
public:
    // ✅ UAV barrier API (all backends must implement)
    virtual void barrierUAV(ITexture* texture) = 0;
    virtual void barrierUAV(IBuffer* buffer) = 0;
    virtual void barrierAllUAVs() = 0;  // Global UAV barrier
};
```

### Step 2: Implement D3D12 UAV Barriers

```cpp
// ComputeCommandEncoder.h
class ComputeCommandEncoder : public IComputeCommandEncoder {
public:
    void barrierUAV(ITexture* texture) override;
    void barrierUAV(IBuffer* buffer) override;
    void barrierAllUAVs() override;

private:
    void insertUAVBarrier(ID3D12Resource* resource);
};

// ComputeCommandEncoder.cpp
void ComputeCommandEncoder::barrierUAV(ITexture* texture) {
    if (!texture) {
        IGL_ASSERT_MSG(false, "Null texture passed to barrierUAV");
        return;
    }

    auto* d3d12Texture = static_cast<Texture*>(texture);
    insertUAVBarrier(d3d12Texture->getD3D12Resource());

#ifdef IGL_DEBUG
    IGL_LOG_DEBUG("UAV barrier inserted for texture: %s", texture->getName());
#endif
}

void ComputeCommandEncoder::barrierUAV(IBuffer* buffer) {
    if (!buffer) {
        IGL_ASSERT_MSG(false, "Null buffer passed to barrierUAV");
        return;
    }

    auto* d3d12Buffer = static_cast<Buffer*>(buffer);
    insertUAVBarrier(d3d12Buffer->getD3D12Resource());

#ifdef IGL_DEBUG
    IGL_LOG_DEBUG("UAV barrier inserted for buffer: %s", buffer->getName());
#endif
}

void ComputeCommandEncoder::barrierAllUAVs() {
    // Global UAV barrier (flushes all UAV caches)
    insertUAVBarrier(nullptr);

#ifdef IGL_DEBUG
    IGL_LOG_DEBUG("Global UAV barrier inserted");
#endif
}

void ComputeCommandEncoder::insertUAVBarrier(ID3D12Resource* resource) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;  // nullptr = global barrier

    commandList_->ResourceBarrier(1, &barrier);
}
```

### Step 3: Automatic UAV Barriers (Optional)

```cpp
// ComputeCommandEncoder.cpp
void ComputeCommandEncoder::dispatchCompute(
    uint32_t threadGroupsX,
    uint32_t threadGroupsY,
    uint32_t threadGroupsZ) {

    commandList_->Dispatch(threadGroupsX, threadGroupsY, threadGroupsZ);

    // ✅ OPTIONAL: Automatic global UAV barrier after each dispatch
    // Can be disabled via flag for performance-critical scenarios
    if (autoUAVBarriers_) {
        barrierAllUAVs();
    }
}

// Enable/disable automatic barriers
void ComputeCommandEncoder::setAutoUAVBarriers(bool enable) {
    autoUAVBarriers_ = enable;
}
```

### Step 4: Vulkan/Metal Implementation (for API parity)

```cpp
// Vulkan ComputeCommandEncoder.cpp
void ComputeCommandEncoder::barrierUAV(ITexture* texture) {
    // Vulkan equivalent: Pipeline barrier with memory dependency
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer_,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// Metal ComputeCommandEncoder.mm
void ComputeCommandEncoder::barrierUAV(ITexture* texture) {
    // Metal equivalent: Memory barrier (automatic in Metal)
    [commandEncoder_ memoryBarrierWithScope:MTLBarrierScopeBuffers];
}
```

---

## Testing Requirements

### Unit Tests

```cpp
TEST(ComputeCommandEncoderTest, UAVBarrierAPI) {
    auto device = createDevice();
    auto cmdBuffer = device->createCommandBuffer();
    auto encoder = cmdBuffer->createComputeCommandEncoder();

    auto buffer = device->createBuffer({
        .length = 1024,
        .usage = BufferUsage::Storage
    });

    // Should not crash
    encoder->barrierUAV(buffer.get());
    encoder->barrierAllUAVs();

    encoder->endEncoding();
}

TEST(ComputeCommandEncoderTest, MultipleDispatches) {
    auto device = createDevice();
    auto cmdBuffer = device->createCommandBuffer();
    auto encoder = cmdBuffer->createComputeCommandEncoder();

    auto uavBuffer = device->createBuffer({...});

    // First dispatch writes to UAV
    encoder->bindComputePipeline(writePipeline);
    encoder->bindBuffer(uavBuffer, 0);
    encoder->dispatchCompute(64, 1, 1);

    // ✅ UAV barrier ensures visibility
    encoder->barrierUAV(uavBuffer.get());

    // Second dispatch reads from same UAV
    encoder->bindComputePipeline(readPipeline);
    encoder->bindBuffer(uavBuffer, 0);
    encoder->dispatchCompute(64, 1, 1);

    encoder->endEncoding();

    // Expected: No validation errors, correct compute results
}
```

### Validation Tests

```bash
# Enable D3D12 GPU-based validation
set D3D12_GPU_BASED_VALIDATION=1

cd build/vs/shell/Debug
./ParticleSystemSession_d3d12.exe

# Expected output:
# ✅ GOOD: No "UAV barrier required" warnings
# ❌ BAD (without fix): "D3D12 WARNING: UAV barrier missing between dispatches"
```

### Visual Tests

```cpp
// ParticleSystem.cpp - UAV barrier test
void ParticleSystem::update() {
    auto encoder = cmdBuffer->createComputeCommandEncoder();

    // Pass 1: Update particle positions (writes to UAV)
    encoder->bindComputePipeline(updatePipeline);
    encoder->bindBuffer(particleBuffer, 0);  // UAV
    encoder->dispatchCompute(numParticles / 64, 1, 1);

    // ✅ CRITICAL: UAV barrier ensures writes are visible
    encoder->barrierUAV(particleBuffer.get());

    // Pass 2: Sort particles (reads + writes same UAV)
    encoder->bindComputePipeline(sortPipeline);
    encoder->bindBuffer(particleBuffer, 0);  // UAV
    encoder->dispatchCompute(numParticles / 64, 1, 1);

    encoder->endEncoding();

    // Expected: Smooth particle motion, no flickering
    // Without barrier: Particles flicker or freeze
}
```

---

## Definition of Done

- [ ] UAV barrier API added to ICommandEncoder (cross-platform)
- [ ] D3D12 implementation complete (barrierUAV methods)
- [ ] Vulkan/Metal implementations added (for API parity)
- [ ] Unit tests pass (UAV barrier insertion)
- [ ] Validation tests show no UAV warnings
- [ ] Particle system test shows correct behavior
- [ ] Documentation added to API headers
- [ ] Performance impact measured (<0.1% for typical workloads)
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **DO NOT** insert UAV barriers unnecessarily (performance cost)
- **MUST** insert barriers for write-after-write and read-after-write hazards
- Global UAV barriers are expensive - prefer specific resource barriers when possible
- Vulkan uses pipeline barriers; Metal handles most barriers automatically
- Consider automatic barriers for debug builds, manual for release

---

## Related Issues

- **H-003**: Texture state tracking race (related synchronization issue)
- **C-004**: Storage buffer synchronous wait (affects compute shader timing)
- **DX12-COD-005**: CBV 64 KB violation (affects compute buffer bindings)
