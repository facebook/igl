# D3D12 Descriptor Race Condition Analysis

## Problem Statement

**BindGroupSession** still crashes with `DXGI_ERROR_DEVICE_HUNG` despite implementing per-frame descriptor heaps. The cube randomly disappears and crashes occur more frequently when FPS drops.

## Root Cause

The issue is **NOT** with the per-frame descriptor heap implementation itself, but with **how descriptors are being created and referenced**:

### Current Flawed Pattern

1. **BindGroup created once at initialization** ([BindGroupSession.cpp:325-329](shell/renderSessions/BindGroupSession.cpp#L325-L329))
   ```cpp
   bindGroupTextures_ = getPlatform().getDevice().createBindGroup(BindGroupTextureDesc{
       {tex0, tex1},
       {sampler, sampler},
       "bindGroupTextures_",
   });
   ```

2. **Every frame, bindBindGroup() creates NEW descriptors** ([RenderCommandEncoder.cpp:708](src/igl/d3d12/RenderCommandEncoder.cpp#L708))
   ```cpp
   const uint32_t descriptorIndex = commandBuffer_.getNextCbvSrvUavDescriptor()++;
   device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
   ```

3. **Descriptor indices get reset every 3 frames** ([CommandQueue.cpp:281](src/igl/d3d12/CommandQueue.cpp#L281))
   ```cpp
   ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor = 0;
   ```

### The Race Condition

```
Frame 0 (currentFrameIndex=0):
  - Creates SRV at descriptor index 0, 1 (for tex0, tex1)
  - GPU Command List 0 references descriptors at index 0, 1
  - Submit to GPU (GPU is executing asynchronously)

Frame 1 (currentFrameIndex=1):
  - Creates SRV at descriptor index 0, 1 (Frame 1's heap, isolated - OK)
  - GPU Command List 1 references descriptors at index 0, 1
  - Submit to GPU

Frame 2 (currentFrameIndex=2):
  - Creates SRV at descriptor index 0, 1 (Frame 2's heap, isolated - OK)
  - GPU Command List 2 references descriptors at index 0, 1
  - Submit to GPU

Frame 3 (currentFrameIndex=0 again):
  - Wait for Frame 0's fence (GPU SHOULD be done with Frame 0)
  - Reset counters: nextCbvSrvUavDescriptor = 0
  - Creates NEW SRV at descriptor index 0, 1 (OVERWRITES Frame 0's descriptors)

  **RACE CONDITION**: If GPU is STILL reading Frame 0's descriptors
  (due to slow FPS, window drag, VSync off), we just overwrote them!

  - GPU reads garbage descriptors → DEVICE_HUNG or visual glitches
```

### Why FPS Matters

- **High FPS (fast)**: GPU finishes Frame 0 before Frame 3 starts → fence wait succeeds quickly
- **Low FPS (slow)**: CPU submits frames faster than GPU executes them → frames pile up
  - Window drag, VSync off, heavy load causes GPU to fall behind
  - By the time Frame 3 starts, GPU might STILL be processing Frame 0
  - Fence wait should catch this, BUT there's a window where descriptors are being read

## Current Fence Synchronization

The fence logic in [CommandQueue.cpp:210-252](src/igl/d3d12/CommandQueue.cpp#L210-L252) DOES wait for the frame's fence before resetting:

```cpp
if (nextFrameFence > fence->GetCompletedValue()) {
    // Wait for GPU to finish this frame before reusing its resources
    WaitForSingleObject(ctx.getFenceEvent(), 10000); // 10 second timeout
}
```

However, the fence only signals when **ALL work for that frame completes**. There's still a race window:
1. Fence signals (GPU finished command list execution)
2. We reset descriptor counters
3. **BUT**: GPU might still be reading descriptors from the render target operations
4. We overwrite descriptors → crash

## Proposed Solutions

### Option 1: Never Reuse Descriptors (Safest)
- Allocate descriptors from a large persistent heap (like 10,000 slots)
- Never reset counters, just wrap around when full
- Simple but wastes memory if scene has few textures

### Option 2: Cache Descriptors Per-Resource (Optimal)
- Create descriptors ONCE per texture/sampler
- Store descriptor GPU handle in the texture/sampler object
- Reuse same descriptor every frame
- **This is what Microsoft MiniEngine does**

### Option 3: Wait for Full GPU Idle (Conservative)
- After fence wait, add `device->WaitForGPU()` to ensure 100% idle
- Safest but kills parallelism (bad for performance)

### Option 4: Increase Frame Lag (Temporary Fix)
- Use 4-5 frames instead of 3
- Gives more buffer time for GPU to finish
- Doesn't fix root cause

## Recommended Fix

**Implement Option 2**: Cache descriptors per-resource

```cpp
// In D3D12Texture.h
class Texture {
  D3D12_GPU_DESCRIPTOR_HANDLE cachedSRVHandle_ = {};  // Cached SRV
  uint32_t cachedSRVIndex_ = UINT32_MAX;  // Index in persistent heap
};

// In RenderCommandEncoder::bindTexture()
if (texture->cachedSRVIndex_ == UINT32_MAX) {
    // First time: allocate from PERSISTENT heap (not per-frame heap)
    texture->cachedSRVIndex_ = device->allocatePersistentDescriptor();
    device->CreateShaderResourceView(..., cpuHandle);
    texture->cachedSRVHandle_ = gpuHandle;
}
// Reuse cached handle
commandList->SetGraphicsRootDescriptorTable(rootIndex, texture->cachedSRVHandle_);
```

## FPS Throttling for Testing

Add a debug mode to artificially slow down frames to trigger the race condition:

```cpp
// In RenderSession or Shell, add environment variable support:
if (const char* throttle = getenv("IGL_FPS_THROTTLE_MS")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(atoi(throttle)));
}
```

Usage:
```batch
set IGL_FPS_THROTTLE_MS=50
.\BindGroupSession_d3d12.exe
```

This forces 50ms delay per frame (20 FPS) to make the race condition deterministic.

## Next Steps

1. Add FPS throttling environment variable to all sessions
2. Verify the race condition triggers consistently with throttling
3. Implement descriptor caching (Option 2)
4. Validate fix with throttling enabled
