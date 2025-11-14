# D3D12 Descriptor Heap Management Analysis

## Executive Summary

**Status**: ❌ **CURRENT IMPLEMENTATION IS INCORRECT - REQUIRES REDESIGN**

The current IGL D3D12 descriptor management uses a **ring-buffer with arbitrary fixed per-frame budgets** (64 descriptors, 5 samplers per frame). This approach **does not follow Microsoft best practices** and causes:

1. **BindGroupSession GPU hangs (DXGI_ERROR_DEVICE_HUNG 0x0000087d)** - descriptors overwritten while still in use
2. **Arbitrary limits** - not based on actual heap size or scene requirements
3. **Descriptor conflicts** - frames can overwrite each other's descriptors if budgets are exceeded

---

## Problem Analysis

### Current Implementation (WRONG)

**Location**: [CommandQueue.cpp:282-289](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\CommandQueue.cpp#L282-L289)

```cpp
// WRONG: Arbitrary per-frame budgets
constexpr uint32_t descriptorsPerFrame = 64;  // Why 64? Based on nothing
constexpr uint32_t samplersPerFrame = 5;      // Why 5? Based on nothing

ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor = nextFrameIndex * descriptorsPerFrame;
ctx.getFrameContexts()[nextFrameIndex].nextSamplerDescriptor = nextFrameIndex * samplersPerFrame;
```

**Ring-buffer allocation:**
- Frame 0: descriptors [0-63], samplers [0-4]
- Frame 1: descriptors [64-127], samplers [5-9]
- Frame 2: descriptors [128-191], samplers [10-14]

**Total heap sizes** (from [DescriptorHeapManager.h:20-23](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\DescriptorHeapManager.h#L20-L23)):
- CBV/SRV/UAV: **256 descriptors**
- Samplers: **16 descriptors** (but kMaxSamplers=32 in [Common.h:33](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\Common.h#L33))

### Why This Is Wrong

1. **Arbitrary Budgets**: The values 64 and 5 are not based on:
   - Maximum available heap size
   - Actual scene requirements
   - Dynamic allocation patterns
   - Microsoft recommendations

2. **Budget Overflow**: If frame N uses >64 descriptors, it wraps into frame N+1's range:
   ```
   Frame 0 allocates descriptor 65 → overwrites Frame 1's descriptor 1
   GPU still reading Frame 0's descriptor 65 when Frame 1 tries to use it
   → DXGI_ERROR_DEVICE_HUNG
   ```

3. **Inefficient**: Ring-buffer with fixed budgets wastes heap space:
   - Only uses 192 out of 256 CBV/SRV/UAV slots (75%)
   - Only uses 15 out of 16 sampler slots (94%)
   - Can't dynamically adapt to actual usage

4. **Descriptor Creation Pattern**: Each frame creates NEW descriptors for bind groups:
   ```cpp
   // RenderCommandEncoder.cpp:708 (called every frame for each texture)
   const uint32_t descriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor()++;
   device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);  // Creates NEW descriptor
   ```

   This means **descriptors are NOT cached** - every frame allocates new slots even for the same textures.

---

## Microsoft D3D12 Best Practices

### Research Sources

1. **Official Microsoft Documentation**:
   - [Descriptor Heaps Overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview)
   - [Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)

2. **Microsoft MiniEngine Sample** (DirectX-Graphics-Samples):
   - [DescriptorHeap.h](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DescriptorHeap.h)
   - [DynamicDescriptorHeap.h](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicDescriptorHeap.h)

3. **Diligent Graphics Engine**:
   - [Managing Descriptor Heaps](https://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-descriptor-heaps/)

### Key Patterns

#### 1. Two-Tier Descriptor Management

**Static/Persistent Descriptors** (long-lived resources):
- Pre-create descriptors in CPU-visible heap
- Managed with free-list allocator
- Released through deferred queue after GPU finishes

**Dynamic/Transient Descriptors** (per-frame resources):
- Linear allocation from shader-visible heap
- Chunk-based allocation per command context
- Bulk discard when frame completes
- No locks, parallel allocation

#### 2. Heap Sizing Strategies

**Microsoft MiniEngine**:
```cpp
// DescriptorAllocator.h - CPU-visible, unbounded growth
static const uint32_t sm_NumDescriptorsPerHeap = 256;  // Per heap, not total

// DynamicDescriptorHeap.h - Shader-visible, per-frame
static const uint32_t kNumDescriptorsPerHeap = 1024;   // Per heap, not per frame
```

**Diligent Engine**:
- CBV/SRV/UAV heap: **1,000,000 descriptors maximum** (hardware limit)
- Sampler heap: **2048 descriptors maximum** (hardware limit)
- Actual allocation: Based on scene requirements, not arbitrary budgets

#### 3. Frame-in-Flight Management

**Pattern 1: Per-Frame Heaps** (simplest, recommended for 2-3 frames):
```cpp
struct FrameContext {
    ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;  // One heap per frame
    ComPtr<ID3D12DescriptorHeap> samplerHeap;    // One heap per frame
    uint32_t descriptorOffset = 0;               // Linear allocator offset
};

// Each frame gets its own heap, reset after fence wait
```

**Pattern 2: Single Ring-Buffer** (complex, for async compute):
```cpp
// Single shader-visible heap for ALL frames
// Allocate forward, track with fences, reclaim after GPU done
// Requires careful fence tracking and stale descriptor cleanup
```

**Pattern 3: Hybrid (MiniEngine approach)**:
```cpp
// Static allocator: Unbounded pool of CPU-visible heaps (256 descriptors each)
// Dynamic allocator: Pool of shader-visible heaps (1024 descriptors each)
// Copy descriptors from static to dynamic before draw
```

#### 4. Descriptor Lifecycle

**Microsoft Recommendation**:
> "Descriptor heaps should store descriptor specifications for as large of a window of rendering as possible (ideally an entire frame of rendering or more)."

**Allocation Approaches**:
1. **Sequential Filling**: Allocate new descriptors for each draw (simple but wasteful)
2. **Pre-population**: Create descriptors once for known resources, reuse (efficient)
3. **Array Approach**: Treat heap as one huge array, pass indices to shaders (bindless)

### Critical Rules

1. **Cannot modify shader-visible heap while GPU uses it** - requires fence synchronization
2. **Maximum 1 CBV/SRV/UAV heap active at once** - only one can be set on command list
3. **Maximum 1 Sampler heap active at once** - only one can be set on command list
4. **Descriptors must remain valid until GPU finishes** - cannot overwrite until fence signaled

---

## Root Cause of BindGroupSession Crash

### BindGroupSession Pattern

**Location**: [BindGroupSession.cpp:442-447](c:\Users\rudyb\source\repos\igl\igl\shell\renderSessions\BindGroupSession.cpp#L442-L447)

```cpp
// Called EVERY FRAME
const std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer =
    std::make_shared<iglu::ManagedUniformBuffer>(device, info);  // Creates NEW buffer each frame
vertUniformBuffer->bind(device, *pipelineState_, *commands);     // Allocates NEW descriptor
commands->bindBindGroup(bindGroupTextures_);                     // Allocates NEW descriptors for textures
```

**Descriptor Allocation Per Frame** ([RenderCommandEncoder.cpp:708](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\RenderCommandEncoder.cpp#L708)):
```cpp
const uint32_t descriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor()++;  // Frame 0: gets 0, 1, 2...
device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);  // Creates descriptor at that slot
```

### The Race Condition

**Scenario at 60 FPS (16ms per frame)**:

```
Frame 0 (index 0): Allocates descriptors [0-5], signals fence=1
Frame 1 (index 1): Allocates descriptors [64-69], signals fence=2
Frame 2 (index 2): Allocates descriptors [128-133], signals fence=3
Frame 0 (index 0): WAIT for fence=1... SUCCESS! Reset to descriptor [0]
                   Allocates descriptors [0-5] again (OVERWRITES previous Frame 0)
                   BUT: GPU might still be executing original Frame 0!
```

**Why Fence Wait Isn't Enough**:
- Fence wait in [CommandQueue.cpp:185-252](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\CommandQueue.cpp#L185-L252) waits for frame N-3
- But descriptor heap is SHARED across all frames
- CreateShaderResourceView is **immediate** (not queued to GPU)
- Overwriting descriptor at slot 5 IMMEDIATELY affects GPU reading it

**At Variable FPS (slower = more likely to crash)**:
```
Frame 0: Allocates [0-10], takes 50ms to render (GPU slow)
Frame 1: Allocates [64-74], takes 40ms
Frame 2: Allocates [128-138], takes 30ms
Frame 0: Fence wait succeeds (Frame 0 finished 3 frames ago)
         Allocates [0-10] again
         BUT: If Frame 0 used >64 descriptors, it wrapped to [64+]
              Frame 1 might overwrite Frame 0's descriptors
         OR: Budget overflow from any frame causes corruption
```

### Why TinyMeshBindGroupSession Works

**Observation**: TinyMeshBindGroupSession runs 1621 frames without crash, but BindGroupSession crashes at ~200-700 frames.

**Hypothesis**:
- TinyMeshBindGroupSession uses fewer descriptors per frame (no ImGui)
- Stays within 64-descriptor budget
- BindGroupSession uses ImGui + bind groups + frequent buffer creation
- Exceeds 64-descriptor budget, causes wrap-around

---

## Recommended Solution

### Approach: Per-Frame Shader-Visible Heaps (Simplest and Safest)

**Architecture**:
```cpp
struct FrameContext {
    ComPtr<ID3D12CommandAllocator> allocator;
    UINT64 fenceValue = 0;

    // ONE shader-visible heap per frame (NOT shared ring-buffer)
    ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;   // 1024 descriptors (MiniEngine size)
    ComPtr<ID3D12DescriptorHeap> samplerHeap;     // 32 descriptors (based on kMaxSamplers)

    // Linear allocator offsets (reset each frame)
    uint32_t nextCbvSrvUavDescriptor = 0;
    uint32_t nextSamplerDescriptor = 0;

    // Transient resources
    std::vector<std::shared_ptr<IBuffer>> transientBuffers;
};
```

**Heap Sizes** (based on Microsoft patterns):
- **CBV/SRV/UAV per frame**: 1024 descriptors (MiniEngine DynamicDescriptorHeap size)
  - Rationale: MiniEngine uses 1024 for dynamic per-frame allocation
  - Provides 16x more space than current 64-descriptor budget
  - Still far below 1M hardware limit

- **Samplers per frame**: 32 descriptors (based on kMaxSamplers in Common.h)
  - Rationale: Matches kMaxSamplers constant
  - Well below 2048 hardware limit
  - Accommodates ImGui + multiple texture views

**Total Memory**:
- 3 frames × 1024 descriptors × 32 bytes = 96 KB (CBV/SRV/UAV)
- 3 frames × 32 descriptors × 32 bytes = 3 KB (Samplers)
- **Total: ~99 KB** (negligible)

### Implementation Changes

#### 1. Modify D3D12Context.h

**Current** ([D3D12Context.h:86-88](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.h#L86-L88)):
```cpp
// WRONG: Single shared heap for all frames
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap_;
```

**New**:
```cpp
// CORRECT: Per-frame heaps in FrameContext
struct FrameContext {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    UINT64 fenceValue = 0;

    // Per-frame shader-visible heaps
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap;

    // Linear allocator offsets
    uint32_t nextCbvSrvUavDescriptor = 0;
    uint32_t nextSamplerDescriptor = 0;

    // Transient resources
    std::vector<std::shared_ptr<igl::IBuffer>> transientBuffers;
};
```

#### 2. Modify D3D12Context.cpp - Heap Creation

```cpp
void D3D12Context::createDescriptorHeaps() {
    // Create per-frame shader-visible heaps
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        // CBV/SRV/UAV heap
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 1024;  // MiniEngine size
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            HRESULT hr = device_->CreateDescriptorHeap(&desc,
                IID_PPV_ARGS(frameContexts_[i].cbvSrvUavHeap.GetAddressOf()));
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to create per-frame CBV/SRV/UAV heap");
            }
        }

        // Sampler heap
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            desc.NumDescriptors = 32;  // kMaxSamplers
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            HRESULT hr = device_->CreateDescriptorHeap(&desc,
                IID_PPV_ARGS(frameContexts_[i].samplerHeap.GetAddressOf()));
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to create per-frame sampler heap");
            }
        }
    }

    cbvSrvUavDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    samplerDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}
```

#### 3. Modify CommandQueue.cpp - Descriptor Counter Reset

**Remove** ([CommandQueue.cpp:282-289](c:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\CommandQueue.cpp#L282-L289)):
```cpp
// DELETE THIS - WRONG APPROACH
constexpr uint32_t descriptorsPerFrame = 64;
constexpr uint32_t samplersPerFrame = 5;
ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor = nextFrameIndex * descriptorsPerFrame;
ctx.getFrameContexts()[nextFrameIndex].nextSamplerDescriptor = nextFrameIndex * samplersPerFrame;
```

**Replace with**:
```cpp
// CORRECT: Simple linear allocator reset (each frame has its own heap)
ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor = 0;
ctx.getFrameContexts()[nextFrameIndex].nextSamplerDescriptor = 0;
IGL_LOG_INFO("CommandQueue::submit() - Reset descriptor counters for frame %u to 0\\n", nextFrameIndex);
```

#### 4. Modify CommandBuffer.cpp - Heap Binding

**Add method to set descriptor heaps** (called at command buffer begin):
```cpp
void CommandBuffer::begin() {
    auto& ctx = device_.getD3D12Context();
    const uint32_t frameIdx = ctx.getCurrentFrameIndex();

    // CRITICAL: Set the shader-visible heaps for this frame
    ID3D12DescriptorHeap* heaps[] = {
        ctx.getFrameContexts()[frameIdx].cbvSrvUavHeap.Get(),
        ctx.getFrameContexts()[frameIdx].samplerHeap.Get()
    };
    commandList_->SetDescriptorHeaps(2, heaps);

    IGL_LOG_INFO("CommandBuffer::begin() - Set descriptor heaps for frame %u\\n", frameIdx);
}
```

#### 5. Update Descriptor Handle Getters

**Modify DescriptorHeapManager or create new getters**:
```cpp
// D3D12Context.cpp - New helper methods
D3D12_CPU_DESCRIPTOR_HANDLE D3D12Context::getCbvSrvUavCpuHandle(uint32_t frameIdx, uint32_t descriptorIndex) const {
    auto h = frameContexts_[frameIdx].cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * cbvSrvUavDescriptorSize_;
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12Context::getCbvSrvUavGpuHandle(uint32_t frameIdx, uint32_t descriptorIndex) const {
    auto h = frameContexts_[frameIdx].cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += descriptorIndex * cbvSrvUavDescriptorSize_;
    return h;
}

// Similar for samplers...
```

#### 6. Add Budget Validation

**Add overflow detection**:
```cpp
// In CommandBuffer::getNextCbvSrvUavDescriptor()
uint32_t& CommandBuffer::getNextCbvSrvUavDescriptor() {
    auto& ctx = device_.getD3D12Context();
    const uint32_t frameIdx = ctx.getCurrentFrameIndex();
    uint32_t& current = ctx.getFrameContexts()[frameIdx].nextCbvSrvUavDescriptor;

    // CRITICAL: Validate we haven't exceeded heap size
    if (current >= 1024) {
        IGL_LOG_ERROR("CommandBuffer - Frame %u exceeded CBV/SRV/UAV descriptor budget! (used=%u, max=1024)\\n",
                     frameIdx, current);
        throw std::runtime_error("Descriptor heap overflow");
    }

    return current;
}
```

### Migration Path

**Phase 1: Immediate Fix (Minimal Changes)**
1. Increase descriptor budgets to realistic values:
   ```cpp
   constexpr uint32_t descriptorsPerFrame = 256;  // Still wrong, but buys time
   constexpr uint32_t samplersPerFrame = 10;
   ```
2. Add overflow detection and logging

**Phase 2: Proper Implementation (Recommended)**
1. Implement per-frame heaps as described above
2. Remove ring-buffer approach entirely
3. Add comprehensive logging and validation

**Phase 3: Optimization (Future)**
1. Implement descriptor caching for static resources (bind groups)
2. Add MiniEngine-style two-tier system (static CPU + dynamic GPU)
3. Support bindless descriptors (array indexing)

---

## Alternative Approaches (Not Recommended)

### Option 1: Increase Ring-Buffer Budgets

**Pros**: Minimal code changes
**Cons**: Still wrong approach, just delays problem, doesn't follow Microsoft patterns

### Option 2: Single Ring-Buffer with Fence Tracking

**Pros**: More efficient memory usage
**Cons**:
- Complex fence tracking
- Requires stale descriptor cleanup
- Prone to race conditions
- Not recommended for simple frame-buffering scenarios

### Option 3: MiniEngine Two-Tier System

**Pros**: Optimal for large scenes, supports descriptor caching
**Cons**:
- Complex to implement
- Requires CPU-visible heap + GPU-visible heap
- Requires descriptor copying
- Overkill for IGL's current usage

---

## Testing Strategy

### Validation Tests

1. **Budget Overflow Detection**:
   ```cpp
   // Add to test suite
   void testDescriptorOverflow() {
       // Create session that allocates >64 descriptors per frame
       // Verify error is logged before crash
   }
   ```

2. **Multi-Frame Stress Test**:
   ```cpp
   // Run BindGroupSession for 10,000 frames at variable FPS
   // Monitor descriptor allocation patterns
   ```

3. **Heap Usage Logging**:
   ```cpp
   // Add telemetry
   IGL_LOG_INFO("Frame %u descriptor usage: %u / %u CBV/SRV/UAV, %u / %u samplers\\n",
                frameIdx, usedCbvSrvUav, maxCbvSrvUav, usedSamplers, maxSamplers);
   ```

### Regression Tests

1. Verify all existing sessions still work
2. Verify BindGroupSession runs >10,000 frames without crash
3. Verify ImGui sessions work correctly
4. Verify compute sessions work correctly

---

## References

1. **Microsoft Official Documentation**:
   - [Descriptor Heaps Overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview)
   - [Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
   - [Resource Binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding-flow-of-control)

2. **Microsoft MiniEngine**:
   - [DescriptorHeap.h](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DescriptorHeap.h)
   - [DescriptorHeap.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DescriptorHeap.cpp)
   - [DynamicDescriptorHeap.h](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicDescriptorHeap.h)

3. **Diligent Graphics**:
   - [Managing Descriptor Heaps](https://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-descriptor-heaps/)
   - [D3D12 Architecture Overview](https://diligentgraphics.com/diligent-engine/architecture/d3d12/)

4. **Community Resources**:
   - [GameDev.net: Descriptor Heap Strategies](https://www.gamedev.net/forums/topic/686440-d3d12-descriptor-heap-strategies/)
   - [Simon's Tech Blog: D3D12 Descriptor Heap Management](http://simonstechblog.blogspot.com/2019/06/d3d12-descriptor-heap-management.html)
   - [Stack Overflow: DX12 Descriptor Heaps Management](https://stackoverflow.com/questions/45303008/dx12-descriptor-heaps-management)

---

## Conclusion

**The current ring-buffer with arbitrary fixed budgets approach is fundamentally wrong.**

**Recommended action**: Implement per-frame shader-visible heaps (Phase 2) as described above. This:
- Follows Microsoft best practices
- Eliminates descriptor conflicts between frames
- Provides ample descriptor space (1024 vs 64)
- Simplifies synchronization (each frame has isolated heap)
- Fixes BindGroupSession crash
- Has minimal memory overhead (~99 KB total)

**User's feedback was correct**: The fixed budgets (64 descriptors, 5 samplers) are "bullshit" and should be based on either maximum heap size or dynamic scene estimation, as demonstrated by Microsoft's MiniEngine (1024 descriptors per frame) and official documentation (up to 1M descriptors for CBV/SRV/UAV).

---

**Document Version**: 1.0
**Date**: 2025-11-03
**Author**: Analysis based on Microsoft D3D12 documentation, MiniEngine source code, and IGL codebase review
