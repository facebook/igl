# G-010: Resource Barriers Causing Pipeline Stalls

**Priority**: MEDIUM
**Category**: Graphics - Performance
**Estimated Effort**: 4-5 days
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend inserts resource barriers inefficiently, causing GPU pipeline stalls. Barriers may be redundant, poorly timed, or not properly batched, forcing the GPU to flush its pipeline and wait for in-flight work to complete before proceeding.

**Symptoms**:
- GPU bubbles (idle time) visible in PIX timeline
- Frame time spikes at barrier insertion points
- Suboptimal GPU utilization
- Excessive number of barrier commands
- Barriers between every draw call or state change

**Impact**:
- Reduced frame rate due to pipeline flushes
- GPU parallelism broken by unnecessary synchronization
- Higher frame time variance
- Poor scaling with complex scenes
- Inability to leverage asynchronous compute or copy

---

## 2. Root Cause Analysis

Resource barrier inefficiencies typically arise from several issues:

### 2.1 Lack of Resource State Tracking
The backend may not track the current state of each resource, causing it to insert conservative barriers "just in case" rather than only when state transitions actually occur.

**Problematic Pattern**:
```cpp
// Barrier before every use, even if already in correct state
barrier(texture, UNKNOWN -> PIXEL_SHADER_RESOURCE);
// Use texture
barrier(texture, PIXEL_SHADER_RESOURCE -> UNKNOWN);
```

### 2.2 No Barrier Batching
Barriers may be inserted one at a time, causing multiple pipeline flushes instead of batching multiple transitions into a single barrier command.

**Current (Inefficient)**:
```cpp
commandList->ResourceBarrier(1, &barrier1);  // Flush
commandList->ResourceBarrier(1, &barrier2);  // Flush
commandList->ResourceBarrier(1, &barrier3);  // Flush
```

**Optimal**:
```cpp
D3D12_RESOURCE_BARRIER barriers[] = {barrier1, barrier2, barrier3};
commandList->ResourceBarrier(3, barriers);  // Single flush
```

### 2.3 Overly Conservative State Transitions
Using `D3D12_RESOURCE_STATE_COMMON` or `D3D12_RESOURCE_STATE_GENERIC_READ` when more specific states would avoid transitions.

### 2.4 Split Barriers Not Used
Split barriers allow the GPU to continue work between the begin and end of a state transition, but they may not be implemented.

### 2.5 Barriers on Every Bind
Resources may transition to/from render target or depth stencil state on every bind, even within the same render pass.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Resource Barriers Overview**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
   - Comprehensive guide to barrier types and usage

2. **Resource State Transitions**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states
   - All resource states and valid transitions

3. **Enhanced Barriers (Agility SDK)**:
   - https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html
   - Modern barrier API with better performance

4. **Split Barriers**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#split-barriers
   - How to use split barriers for async operations

5. **Performance Best Practices**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/performance-tips#resource-barriers
   - Microsoft's barrier optimization guidelines

**Sample Code**:

6. **MiniEngine Resource State Tracking**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/GpuResource.cpp
   - Reference implementation of state tracking

7. **D3D12 Raytracing Sample - Barrier Management**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Raytracing
   - Advanced barrier batching patterns

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for barrier insertion**:
```
Pattern: "ResourceBarrier" OR "D3D12_RESOURCE_BARRIER"
Files: src/igl/d3d12/**/*.cpp
Focus: Every location where barriers are inserted
```

**Search for state tracking**:
```
Pattern: "D3D12_RESOURCE_STATES" OR "currentState" OR "resourceState"
Files: src/igl/d3d12/Texture.cpp, src/igl/d3d12/Buffer.cpp
Focus: How resource states are tracked (if at all)
```

**Search for render target binding**:
```
Pattern: "OMSetRenderTargets" OR "BeginRenderPass"
Files: src/igl/d3d12/RenderCommandEncoder.cpp
Focus: Barriers around render target transitions
```

**Search for texture binding**:
```
Pattern: "SetGraphicsRootDescriptorTable" OR "bindTexture"
Files: src/igl/d3d12/**/*.cpp
Focus: Barriers around shader resource transitions
```

### 4.2 Likely File Locations

Based on typical D3D12 backend architecture:
- `src/igl/d3d12/Texture.cpp` - Texture resource state management
- `src/igl/d3d12/Buffer.cpp` - Buffer resource state management
- `src/igl/d3d12/RenderCommandEncoder.cpp` - Barriers during rendering
- `src/igl/d3d12/CommandBuffer.cpp` - Barrier batching logic
- `src/igl/d3d12/Framebuffer.cpp` - Render target barriers
- `src/igl/d3d12/ComputeCommandEncoder.cpp` - Compute barriers

### 4.3 Key Data Structures to Find

Look for:
- Resource state tracking per resource (should be per-subresource)
- Barrier batching container (e.g., `std::vector<D3D12_RESOURCE_BARRIER>`)
- State transition validation logic
- Barrier insertion points in command recording

---

## 5. Detection Strategy

### 5.1 Add Instrumentation Code

**Step 1: Count and categorize barriers**

Add global statistics:

```cpp
// Barrier statistics
struct BarrierStats {
    std::atomic<uint64_t> totalBarriers{0};
    std::atomic<uint64_t> transitionBarriers{0};
    std::atomic<uint64_t> aliasingBarriers{0};
    std::atomic<uint64_t> uavBarriers{0};
    std::atomic<uint64_t> redundantBarriers{0};  // Same state transition
    std::atomic<uint64_t> batchedBarriers{0};    // Multiple in one call

    void logStats() {
        IGL_LOG_INFO("Barrier Statistics:\n");
        IGL_LOG_INFO("  Total Barriers: %llu\n", totalBarriers.load());
        IGL_LOG_INFO("  Transition: %llu\n", transitionBarriers.load());
        IGL_LOG_INFO("  Aliasing: %llu\n", aliasingBarriers.load());
        IGL_LOG_INFO("  UAV: %llu\n", uavBarriers.load());
        IGL_LOG_INFO("  Redundant: %llu\n", redundantBarriers.load());
        IGL_LOG_INFO("  Batched: %llu\n", batchedBarriers.load());
    }
};

static BarrierStats g_barrierStats;
```

**Step 2: Instrument barrier insertion**

```cpp
// Wrap all ResourceBarrier calls
void insertBarriers(ID3D12GraphicsCommandList* cmdList,
                    UINT numBarriers,
                    D3D12_RESOURCE_BARRIER* barriers) {
    g_barrierStats.totalBarriers += numBarriers;

    if (numBarriers > 1) {
        g_barrierStats.batchedBarriers++;
    }

    for (UINT i = 0; i < numBarriers; ++i) {
        if (barriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
            g_barrierStats.transitionBarriers++;

            // Detect redundant barriers
            if (barriers[i].Transition.StateBefore == barriers[i].Transition.StateAfter) {
                g_barrierStats.redundantBarriers++;
                IGL_LOG_WARNING("Redundant barrier: %s -> %s\n",
                    getStateName(barriers[i].Transition.StateBefore),
                    getStateName(barriers[i].Transition.StateAfter));
            }
        } else if (barriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {
            g_barrierStats.aliasingBarriers++;
        } else if (barriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
            g_barrierStats.uavBarriers++;
        }
    }

    cmdList->ResourceBarrier(numBarriers, barriers);
}
```

**Step 3: Track resource states**

```cpp
// Per-resource state tracking
class Resource {
private:
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;

public:
    D3D12_RESOURCE_STATES getCurrentState() const { return currentState_; }

    void transitionTo(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES newState) {
        if (currentState_ != newState) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource_.Get();
            barrier.Transition.StateBefore = currentState_;
            barrier.Transition.StateAfter = newState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            cmdList->ResourceBarrier(1, &barrier);
            currentState_ = newState;

            IGL_LOG_DEBUG("Barrier: %s -> %s\n",
                getStateName(currentState_),
                getStateName(newState));
        } else {
            IGL_LOG_DEBUG("Skipped redundant barrier to %s\n", getStateName(newState));
        }
    }
};
```

### 5.2 Manual Testing Procedure

**Test 1: Run tests with barrier logging**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

Analyze console output for barrier patterns.

**Test 2: Profile single frame with PIX**
```bash
# Capture frame in PIX
# Look for:
# - Number of ResourceBarrier calls
# - GPU timeline bubbles after barriers
# - Redundant state transitions
```

**Test 3: Run complex rendering samples**
```bash
./build/Debug/samples/MRTSession
./build/Debug/samples/TQSession
```

Count barriers per frame and look for optimization opportunities.

### 5.3 Expected Baseline Metrics

**Problematic Indicators**:
- >100 barrier calls per frame in simple scenes
- Many single-barrier calls (not batched)
- Redundant transitions (same state -> same state)
- Barriers before every draw call
- Visible GPU bubbles in PIX after each barrier

**Optimized Targets**:
- <50 barrier calls per frame in simple scenes
- Most barriers batched (3-10 per batch)
- Zero redundant transitions
- Barriers only at logical boundaries (render pass changes)
- Minimal GPU bubbles

---

## 6. Fix Guidance

### 6.1 Implement Resource State Tracking

**Step 1: Add state tracking to resource classes**

Find Texture and Buffer classes, add state tracking:

```cpp
// In Texture.h or Buffer.h
class Texture {
private:
    ComPtr<ID3D12Resource> resource_;

    // Track state per subresource (mip level, array slice)
    std::vector<D3D12_RESOURCE_STATES> subresourceStates_;

    // Or track global state if no subresource transitions needed
    D3D12_RESOURCE_STATES globalState_ = D3D12_RESOURCE_STATE_COMMON;

public:
    // Initialize state tracking
    void initializeState(D3D12_RESOURCE_STATES initialState) {
        if (useSubresourceTracking()) {
            UINT numSubresources = numMipLevels_ * arraySize_;
            subresourceStates_.resize(numSubresources, initialState);
        } else {
            globalState_ = initialState;
        }
    }

    // Get current state
    D3D12_RESOURCE_STATES getState(UINT subresource = 0) const {
        if (useSubresourceTracking()) {
            return subresourceStates_[subresource];
        }
        return globalState_;
    }

    // Update state (called after barrier is inserted)
    void setState(D3D12_RESOURCE_STATES newState, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
            if (useSubresourceTracking()) {
                std::fill(subresourceStates_.begin(), subresourceStates_.end(), newState);
            } else {
                globalState_ = newState;
            }
        } else {
            subresourceStates_[subresource] = newState;
        }
    }

private:
    bool useSubresourceTracking() const {
        // Use subresource tracking if we have multiple mips/array slices
        // and they might be in different states
        return numMipLevels_ > 1 || arraySize_ > 1;
    }
};
```

**Step 2: Initialize states correctly**

```cpp
// In Texture/Buffer creation
Result Texture::create(const TextureDesc& desc) {
    // ... create D3D12 resource ...

    // Determine initial state based on usage
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    if (desc.usage & TextureUsageBits::Sampled) {
        initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    } else if (desc.usage & TextureUsageBits::Storage) {
        initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    } else if (desc.usage & TextureUsageBits::Attachment) {
        if (isDepthFormat(desc.format)) {
            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        } else {
            initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
    }

    initializeState(initialState);
    return Result();
}
```

### 6.2 Implement Barrier Batching

Create a barrier batcher class:

```cpp
// BarrierBatcher.h
class BarrierBatcher {
public:
    // Add a transition barrier
    void transitionBarrier(ID3D12Resource* resource,
                          D3D12_RESOURCE_STATES stateBefore,
                          D3D12_RESOURCE_STATES stateAfter,
                          UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
        // Skip redundant barriers
        if (stateBefore == stateAfter) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
        barrier.Transition.Subresource = subresource;

        barriers_.push_back(barrier);
    }

    // Add a UAV barrier
    void uavBarrier(ID3D12Resource* resource) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;

        barriers_.push_back(barrier);
    }

    // Flush all batched barriers
    void flush(ID3D12GraphicsCommandList* commandList) {
        if (!barriers_.empty()) {
            commandList->ResourceBarrier(static_cast<UINT>(barriers_.size()), barriers_.data());

            IGL_LOG_DEBUG("Flushed %zu batched barriers\n", barriers_.size());

            barriers_.clear();
        }
    }

    // Check if any barriers are pending
    bool hasPendingBarriers() const {
        return !barriers_.empty();
    }

    // Get count for statistics
    size_t getPendingCount() const {
        return barriers_.size();
    }

private:
    std::vector<D3D12_RESOURCE_BARRIER> barriers_;
};
```

### 6.3 Integrate State Tracking with Barrier Insertion

Modify command encoder to use state tracking:

```cpp
// In RenderCommandEncoder or CommandBuffer
class RenderCommandEncoder {
private:
    BarrierBatcher barrierBatcher_;

public:
    // Transition resource to required state
    void transitionResource(Texture* texture, D3D12_RESOURCE_STATES requiredState) {
        D3D12_RESOURCE_STATES currentState = texture->getState();

        if (currentState != requiredState) {
            barrierBatcher_.transitionBarrier(
                texture->getResource(),
                currentState,
                requiredState
            );

            // Update tracked state
            texture->setState(requiredState);
        }
    }

    // Bind texture as shader resource
    void bindTexture(Texture* texture, uint32_t slot) {
        // Ensure texture is in correct state
        transitionResource(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // Flush barriers before binding
        barrierBatcher_.flush(commandList_.Get());

        // Now bind the texture
        // ...
    }

    // Begin render pass
    void beginRenderPass(const RenderPassDesc& desc) {
        // Transition all render targets
        for (auto& attachment : desc.colorAttachments) {
            if (attachment.texture) {
                transitionResource(attachment.texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
        }

        if (desc.depthAttachment.texture) {
            transitionResource(desc.depthAttachment.texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }

        // Flush all batched barriers at once
        barrierBatcher_.flush(commandList_.Get());

        // Set render targets
        // ...
    }

    // End render pass
    void endRenderPass() {
        // Transition render targets to common or pixel shader resource
        // if they'll be used as textures later
        // (or defer until actual use)
    }
};
```

### 6.4 Optimize Common Patterns

**Pattern 1: Avoid COMMON state ping-pong**

```cpp
// BAD: Transitioning to/from COMMON frequently
texture->transitionTo(D3D12_RESOURCE_STATE_COMMON);
texture->transitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

// GOOD: Keep in most commonly used state
// Only transition when actually needed for a different purpose
```

**Pattern 2: Use simultaneous access states**

```cpp
// If a texture is only read from multiple shaders, use combined state
D3D12_RESOURCE_STATES readState =
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

// No barrier needed between vertex shader read and pixel shader read
```

**Pattern 3: Defer barriers until last moment**

```cpp
// Instead of transitioning at end of render pass:
void endRenderPass() {
    // Don't transition back to COMMON here
    // Leave in RENDER_TARGET state
}

// Transition when actually needed:
void bindTextureForSampling(Texture* texture) {
    // Only now transition to PIXEL_SHADER_RESOURCE
    transitionResource(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
```

### 6.5 Implement Split Barriers (Advanced)

For async operations:

```cpp
// Begin split barrier (GPU can continue work)
void beginSplitBarrier(ID3D12Resource* resource,
                       D3D12_RESOURCE_STATES beforeState,
                       D3D12_RESOURCE_STATES afterState) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = beforeState;
    barrier.Transition.StateAfter = afterState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList_->ResourceBarrier(1, &barrier);
}

// End split barrier (must complete before using resource)
void endSplitBarrier(ID3D12Resource* resource,
                     D3D12_RESOURCE_STATES beforeState,
                     D3D12_RESOURCE_STATES afterState) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = beforeState;
    barrier.Transition.StateAfter = afterState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList_->ResourceBarrier(1, &barrier);
}

// Usage:
void renderScene() {
    // Start transitioning early
    beginSplitBarrier(backbuffer, PRESENT, RENDER_TARGET);

    // Do other work that doesn't need backbuffer
    updateBuffers();
    bindShaders();

    // Complete transition before use
    endSplitBarrier(backbuffer, PRESENT, RENDER_TARGET);

    // Now use backbuffer
    draw();
}
```

---

## 7. Testing Requirements

### 7.1 Functional Testing

**Test 1: Verify all tests pass**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: All existing tests pass, no visual regressions.

**Test 2: Texture state tests**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Expected**: Textures render correctly in all usage scenarios.

**Test 3: Render target tests**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Framebuffer*"
```

**Expected**: Render targets work correctly with optimized barriers.

**Test 4: Complex rendering samples**
```bash
./build/Debug/samples/MRTSession
./build/Debug/samples/TQSession
```

**Expected**: No visual artifacts or validation errors.

### 7.2 Performance Testing

**Test 1: Count barriers per frame**

Check instrumentation output:
```
Barrier Statistics:
  Total Barriers: 45 (was: 250)
  Redundant: 0 (was: 80)
  Batched: 15 (was: 5)
```

**Test 2: PIX analysis**

Capture frame and verify:
- Fewer ResourceBarrier calls
- More batched barriers (multiple barriers per call)
- Smaller GPU bubbles
- Better GPU utilization

**Target Metrics**:
- 50-80% reduction in total barriers
- Zero redundant barriers
- >90% of barriers batched
- Measurable GPU utilization improvement

### 7.3 Validation Testing

**Enable D3D12 debug layer and GPU validation**:

```cpp
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();

        // Enable GPU-based validation
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1))) {
            debugController1->SetEnableGPUBasedValidation(TRUE);
        }
    }
#endif
```

Run tests and verify no validation errors about:
- Invalid resource states
- Missing barriers
- Resource hazards

### 7.4 Test Modification Restrictions

**CRITICAL CONSTRAINTS**:
- Do NOT modify any existing test assertions
- Do NOT change test expected values
- Do NOT alter test logic or flow
- Changes must be implementation-only, not test-visible
- All tests must pass without modification

If validation errors occur, fix the barrier logic, not the tests.

---

## 8. Definition of Done

### 8.1 Implementation Checklist

- [ ] Resource state tracking implemented for Texture class
- [ ] Resource state tracking implemented for Buffer class
- [ ] Barrier batcher class implemented
- [ ] Barrier batching integrated into command encoding
- [ ] Redundant barrier elimination implemented
- [ ] State initialization logic correct for all resource types
- [ ] Barrier instrumentation added
- [ ] Statistics logging implemented

### 8.2 Testing Checklist

- [ ] All unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"`
- [ ] No D3D12 validation errors with debug layer enabled
- [ ] No GPU-based validation errors
- [ ] Barrier statistics show significant reduction
- [ ] Zero redundant barriers reported
- [ ] No visual regressions in sample applications
- [ ] PIX analysis shows improved GPU utilization

### 8.3 Performance Checklist

- [ ] Total barriers per frame reduced by >50%
- [ ] Redundant barriers eliminated (0%)
- [ ] Barrier batching >90%
- [ ] GPU bubbles reduced in PIX timeline
- [ ] Frame time improved or stable (not regressed)

### 8.4 Documentation Checklist

- [ ] State tracking strategy documented
- [ ] Barrier batching points documented
- [ ] Common barrier patterns documented

### 8.5 Sign-Off Criteria

**Before proceeding with this task, YOU MUST**:
1. Read and understand all 11 sections of this document
2. Understand D3D12 resource states and transition rules
3. Be able to locate barrier insertion points in code
4. Understand the difference between batched and unbatched barriers
5. Confirm you can build and run the test suite
6. Get explicit approval to proceed with implementation

**Do not make any code changes until all criteria are met and approval is given.**

---

## 9. Related Issues

### 9.1 Blocking Issues
None - this can be implemented independently.

### 9.2 Blocked Issues
- Improved barrier efficiency may expose other performance bottlenecks
- Required for efficient async copy (G-009)

### 9.3 Related Performance Issues
- G-008: PSO cache miss rate (both affect frame time)
- G-009: Copy operations not async (requires proper barrier coordination)
- P0 DX12-004: copyTextureToBuffer (needs correct barriers)

---

## 10. Implementation Priority

**Priority Level**: MEDIUM

**Rationale**:
- Significant performance improvement potential
- Reduces GPU stalls and improves utilization
- Moderate implementation complexity
- Low risk if state tracking is correct
- Standard D3D12 best practice

**Recommended Order**:
1. Add instrumentation to measure baseline (Day 1)
2. Implement resource state tracking (Day 2)
3. Implement barrier batcher (Day 2)
4. Integrate state tracking with encoders (Day 3)
5. Test and validate (Day 4)
6. Performance analysis and refinement (Day 5)

**Estimated Timeline**:
- Day 1: Investigation, instrumentation, baseline measurements
- Day 2: State tracking and barrier batcher implementation
- Day 3: Integration with command encoding
- Day 4: Testing and validation
- Day 5: Performance analysis, optimization, final validation

---

## 11. References

### 11.1 Microsoft Official Documentation
- Resource Barriers: https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
- Resource States: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states
- Enhanced Barriers: https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html
- Split Barriers: https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#split-barriers
- Performance Tips: https://docs.microsoft.com/en-us/windows/win32/direct3d12/performance-tips#resource-barriers

### 11.2 Sample Code References
- MiniEngine GpuResource: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/GpuResource.cpp
  - Excellent state tracking implementation
- D3D12 Raytracing Sample: https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Raytracing
  - Advanced barrier management

### 11.3 Additional Reading
- PIX for Windows: https://devblogs.microsoft.com/pix/
- Resource Barrier Optimization: https://therealmjp.github.io/posts/breaking-down-barriers-part-1-whats-a-barrier/
- Barrier Best Practices: https://gpuopen.com/learn/understanding-vulkan-objects/

### 11.4 Internal Codebase
- Search for "ResourceBarrier" in src/igl/d3d12/
- Review current state tracking (if any) in Texture.cpp and Buffer.cpp
- Check barrier patterns in RenderCommandEncoder.cpp

---

**Document Version**: 1.0
**Last Updated**: 2025-11-12
**Author**: Development Team
**Reviewer**: [Pending]
