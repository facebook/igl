# G-001: Resource Barriers Issued Individually Instead of Batched

**Severity:** Medium
**Category:** GPU Performance
**Status:** Open
**Related Issues:** G-003 (Heap Fragmentation), COD-003 (Texture State Management)

---

## Problem Statement

Resource barriers are frequently issued individually via single-barrier `ResourceBarrier(1, &barrier)` calls throughout the codebase, rather than being batched together. D3D12 best practices recommend batching multiple barriers into a single API call to reduce command list overhead and allow the GPU driver to optimize barrier execution.

**Impact Analysis:**
- **Command List Overhead:** Each `ResourceBarrier` call has CPU overhead for validation and command buffer encoding
- **Driver Optimization:** Batching barriers allows the GPU driver to analyze dependencies and potentially parallelize or reorder transitions
- **Cache Efficiency:** Multiple small API calls thrash instruction cache versus one batched call

**Performance Impact:**
- Estimated 5-10% command encoding overhead in barrier-heavy scenarios (texture updates, multi-pass rendering)
- More significant on CPU-bound applications with many resource transitions per frame

**The Danger:**
- Cumulative overhead across hundreds of barriers per frame adds up
- GPU may serialize transitions that could run in parallel
- Command list building becomes a bottleneck in complex scenes

---

## Root Cause Analysis

### Current Implementation (`Texture.cpp:1191-1224`):

```cpp
void Texture::transition(ID3D12GraphicsCommandList* commandList,
                         D3D12_RESOURCE_STATES newState,
                         uint32_t mipLevel,
                         uint32_t layer) const {
  // ... state validation ...

  // PROBLEM: Single barrier issued immediately
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource_.Get();
  barrier.Transition.Subresource = subresource;
  barrier.Transition.StateBefore = currentState;
  barrier.Transition.StateAfter = newState;
  commandList->ResourceBarrier(1, &barrier); // <-- Individual barrier call!
  currentState = newState;
}
```

**Good Example in Same File (`Texture.cpp:1238-1257`):**
```cpp
void Texture::transitionAll(ID3D12GraphicsCommandList* commandList,
                            D3D12_RESOURCE_STATES newState) const {
  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  barriers.reserve(subresourceStates_.size());

  // Build barrier array
  for (uint32_t i = 0; i < subresourceStates_.size(); ++i) {
    if (state == newState) continue;
    D3D12_RESOURCE_BARRIER barrier = {};
    // ... fill barrier ...
    barriers.push_back(barrier);
  }

  // GOOD: Batch all barriers in one call
  if (!barriers.empty()) {
    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
  }
}
```

**Other Problem Locations:**

**`Texture.cpp:554` and `576`** - Mipmap generation:
```cpp
// Transition old resource to COPY_SOURCE
D3D12_RESOURCE_BARRIER barrierOld = {};
// ... fill barrier ...
copyList->ResourceBarrier(1, &barrierOld); // Individual call

// ... copy operation ...

// Transition new resource to PIXEL_SHADER_RESOURCE
D3D12_RESOURCE_BARRIER barrierNew = {};
// ... fill barrier ...
copyList->ResourceBarrier(1, &barrierNew); // Individual call

// MISSED OPPORTUNITY: Could batch both barriers!
```

### Why This Is Wrong:

1. **Individual Barriers:** Most texture transitions issue barriers one at a time, preventing driver optimization
2. **No Batching Infrastructure:** No command encoder-level barrier queue to accumulate barriers
3. **Immediate Submission:** Barriers are submitted immediately upon transition request, not deferred until draw/dispatch
4. **Missed Optimization in Known Patterns:** Mipmap generation has predictable barrier pairs that should be batched

---

## Official Documentation References

1. **Resource Barriers Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#best-practices
   - Key guidance: "Submit multiple resource barriers in a single call whenever possible. Batching reduces overhead and allows drivers to optimize."

2. **Multi-Engine Synchronization**:
   - https://learn.microsoft.com/windows/win32/direct3d12/user-mode-heap-synchronization
   - Guidance: "Group related barriers together to give the driver visibility into dependencies"

3. **Performance Optimization**:
   - https://developer.nvidia.com/blog/dx12-dos-and-donts/
   - NVIDIA guidance: "Batch barriers to reduce command list overhead and enable driver analysis"

4. **DirectX-Graphics-Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3DX12.h
   - Sample demonstrates barrier batching in complex scenarios

---

## Code Location Strategy

### Files to Modify:

1. **Texture.cpp** (`transition` method):
   - Search for: `commandList->ResourceBarrier(1, &barrier);` in single-subresource transition
   - Context: Individual barrier issued for texture state transition
   - Action: Document that immediate barrier is acceptable for single transitions; focus batching on multi-transition scenarios

2. **Texture.cpp** (`generateMipmap` method):
   - Search for: Two consecutive `ResourceBarrier(1,` calls around line 554 and 576
   - Context: Mipmap generation issues two barriers that could be batched
   - Action: Combine into single batched barrier call

3. **RenderCommandEncoder.h/cpp** (new barrier batching infrastructure):
   - Search for: Class member variables section
   - Context: Add barrier queue for deferred submission
   - Action: Add `std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers_` member and flush method

4. **CommandBuffer.cpp** (barrier flush at submission):
   - Search for: `ExecuteCommandLists` call
   - Context: Command list submission point
   - Action: Ensure pending barriers are flushed before command list close

---

## Detection Strategy

### How to Reproduce:

Enable D3D12 debug layer and count `ResourceBarrier` calls:
```cpp
// In test scenario with multiple texture transitions
for (int i = 0; i < 10; ++i) {
  texture->transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, 0);
  texture->transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0);
}
// Expected with batching: 1-2 ResourceBarrier calls total
// Actual without batching: 20 ResourceBarrier calls
```

### Verification After Fix:

1. Run with D3D12 command list logging
2. Count `ResourceBarrier` calls in complex scene
3. Verify barrier count is reduced by ~50-80% in multi-transition scenarios
4. Benchmark command list encoding time shows measurable improvement

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Fix Obvious Batching Opportunity in generateMipmap()

**File:** `src/igl/d3d12/Texture.cpp`

**Locate:** Search for `copyList->ResourceBarrier(1, &barrierOld);` around line 554

**Current (PROBLEM):**
```cpp
// Transition old resource to COPY_SOURCE
D3D12_RESOURCE_BARRIER barrierOld = {};
barrierOld.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrierOld.Transition.pResource = oldResource.Get();
barrierOld.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
barrierOld.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
barrierOld.Transition.Subresource = 0;
copyList->ResourceBarrier(1, &barrierOld); // Individual barrier #1

// Copy mip 0
D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
// ... copy location setup ...
copyList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

// Transition new resource to PIXEL_SHADER_RESOURCE for mipmap generation
D3D12_RESOURCE_BARRIER barrierNew = {};
barrierNew.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrierNew.Transition.pResource = newResource.Get();
barrierNew.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
barrierNew.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
barrierNew.Transition.Subresource = 0;
copyList->ResourceBarrier(1, &barrierNew); // Individual barrier #2
```

**Fixed (SOLUTION):**
```cpp
// G-001: Batch pre-copy and post-copy barriers for efficiency
D3D12_RESOURCE_BARRIER barriers[2] = {};

// Transition old resource to COPY_SOURCE
barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
barriers[0].Transition.pResource = oldResource.Get();
barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
barriers[0].Transition.Subresource = 0;

// Transition new resource from initial COPY_DEST (implicit) to COPY_DEST (explicit)
// This allows batching but doesn't change the actual state
// We'll issue the COPY_SOURCE barrier, do the copy, then batch the post-copy barrier

// Actually, we need barriers BEFORE and AFTER the copy, so split appropriately:
// BEFORE: Old resource -> COPY_SOURCE
copyList->ResourceBarrier(1, &barriers[0]);

// Copy mip 0
D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
srcLoc.pResource = oldResource.Get();
srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
srcLoc.SubresourceIndex = 0;

D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
dstLoc.pResource = newResource.Get();
dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
dstLoc.SubresourceIndex = 0;

copyList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

// AFTER: Batch barriers to transition both resources to their final states
D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {};

// Transition old resource back to PIXEL_SHADER_RESOURCE (if needed for cleanup)
// Actually, old resource is being replaced, so skip this barrier

// Transition new resource to PIXEL_SHADER_RESOURCE for mipmap generation
postCopyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
postCopyBarriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
postCopyBarriers[0].Transition.pResource = newResource.Get();
postCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
postCopyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
postCopyBarriers[0].Transition.Subresource = 0;

// Issue post-copy barrier (only one needed since old resource is discarded)
copyList->ResourceBarrier(1, &postCopyBarriers[0]);
```

**Rationale:** In this specific case, there's a copy operation between the two barriers, so true batching isn't possible. However, document the pattern and ensure future multi-barrier scenarios are batched. The real optimization here is minimal since barriers must straddle the copy.

**Better Approach - Focus on Other Barriers:**

Actually, after analysis, the generateMipmap barriers CANNOT be batched because they straddle a copy operation (barrier 1 -> copy -> barrier 2). The real optimization opportunity is elsewhere.

---

#### Step 2: Add Barrier Batching Infrastructure to CommandEncoder

**File:** `src/igl/d3d12/RenderCommandEncoder.h`

**Locate:** Search for private member variables section

**Current (PROBLEM):**
```cpp
class RenderCommandEncoder : public IRenderCommandEncoder {
 private:
  CommandBuffer& commandBuffer_;
  ID3D12GraphicsCommandList* commandList_ = nullptr;
  // ... other members ...

  // No barrier batching infrastructure!
};
```

**Fixed (SOLUTION):**
```cpp
class RenderCommandEncoder : public IRenderCommandEncoder {
 private:
  CommandBuffer& commandBuffer_;
  ID3D12GraphicsCommandList* commandList_ = nullptr;
  // ... other members ...

  // G-001: Barrier batching infrastructure
  std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers_;

 public:
  // Batch a resource barrier for deferred submission
  void batchBarrier(const D3D12_RESOURCE_BARRIER& barrier);

  // Flush all pending barriers to command list
  void flushBarriers();
};
```

**Rationale:** Provides infrastructure for accumulating barriers and submitting in batches. Encoders can collect barriers during binding operations and flush before draw calls.

---

#### Step 3: Implement Barrier Batching Methods

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Locate:** End of file, before namespace close

**Add New Methods:**
```cpp
void RenderCommandEncoder::batchBarrier(const D3D12_RESOURCE_BARRIER& barrier) {
  // G-001: Add barrier to pending batch
  pendingBarriers_.push_back(barrier);

  // Optional: Auto-flush if batch gets large (prevent unbounded growth)
  constexpr size_t kMaxBatchSize = 64; // Reasonable limit
  if (pendingBarriers_.size() >= kMaxBatchSize) {
    IGL_LOG_INFO("RenderCommandEncoder: Auto-flushing barrier batch (size=%zu)\n",
                 pendingBarriers_.size());
    flushBarriers();
  }
}

void RenderCommandEncoder::flushBarriers() {
  // G-001: Submit all pending barriers in one call
  if (pendingBarriers_.empty()) {
    return;
  }

  if (!commandList_) {
    IGL_LOG_ERROR("RenderCommandEncoder::flushBarriers: Command list is null\n");
    pendingBarriers_.clear();
    return;
  }

  IGL_LOG_INFO("RenderCommandEncoder: Flushing %zu batched barriers\n",
               pendingBarriers_.size());

  commandList_->ResourceBarrier(static_cast<UINT>(pendingBarriers_.size()),
                                 pendingBarriers_.data());

  pendingBarriers_.clear();
}
```

**Rationale:**
- `batchBarrier()` accumulates barriers for later submission
- `flushBarriers()` issues all pending barriers in single API call
- Auto-flush at 64 barriers prevents unbounded growth (tunable threshold)

---

#### Step 4: Flush Barriers at Critical Points

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Locate:** Search for `bindRenderPipelineState`, `drawIndexed`, and `draw` methods

**Current (PROBLEM):**
```cpp
void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t firstInstance) {
  // ... pipeline state checks ...

  // Draw immediately - no barrier flush!
  commandList_->DrawIndexedInstanced(
      static_cast<UINT>(indexCount),
      instanceCount,
      firstIndex,
      vertexOffset,
      firstInstance);
}
```

**Fixed (SOLUTION):**
```cpp
void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t firstInstance) {
  // ... pipeline state checks ...

  // G-001: Flush pending barriers before draw
  flushBarriers();

  commandList_->DrawIndexedInstanced(
      static_cast<UINT>(indexCount),
      instanceCount,
      firstIndex,
      vertexOffset,
      firstInstance);
}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  // ... pipeline state checks ...

  // G-001: Flush pending barriers before draw
  flushBarriers();

  commandList_->DrawInstanced(
      static_cast<UINT>(vertexCount),
      instanceCount,
      firstVertex,
      firstInstance);
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  // ... existing binding logic ...

  // G-001: Flush barriers after pipeline state change (before any draws)
  flushBarriers();
}
```

**Rationale:** Ensure all pending resource transitions are submitted before GPU work (draws/dispatches). Flushing at pipeline state binding ensures correct state before draw calls.

---

#### Step 5: Update Texture Transition to Use Batching (Optional Enhancement)

**File:** `src/igl/d3d12/Texture.cpp`

**Locate:** `transition` method

**Current (keeps immediate submission for now):**
```cpp
void Texture::transition(ID3D12GraphicsCommandList* commandList,
                         D3D12_RESOURCE_STATES newState,
                         uint32_t mipLevel,
                         uint32_t layer) const {
  // ... state checks ...

  D3D12_RESOURCE_BARRIER barrier = {};
  // ... fill barrier ...
  commandList->ResourceBarrier(1, &barrier); // Keep immediate for simplicity
  currentState = newState;
}
```

**Enhanced (FUTURE - Requires encoder context):**
```cpp
// This would require passing RenderCommandEncoder* instead of raw command list
// Deferred for future enhancement to avoid breaking API

// FUTURE: Add method signature:
// void Texture::transitionBatched(RenderCommandEncoder* encoder,
//                                  D3D12_RESOURCE_STATES newState,
//                                  uint32_t mipLevel,
//                                  uint32_t layer);
```

**Rationale:** Keep immediate barrier submission in Texture class for now. The encoder-level batching catches most multi-transition scenarios without changing Texture API.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Rendering tests (ensure barriers still work correctly)
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Render*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Test Modifications Allowed:**
- ✅ Add test to verify barrier batching reduces API call count
- ✅ Add test for flushBarriers() method
- ❌ **DO NOT modify cross-platform test logic**

**New Test to Add:**
```cpp
// Test barrier batching reduces API calls
TEST(RenderCommandEncoder, BatchesBarriersBeforeDraw) {
  auto encoder = createTestRenderEncoder();

  // Batch multiple barriers
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  // ... fill barrier ...

  encoder->batchBarrier(barrier);
  encoder->batchBarrier(barrier); // Duplicate for testing
  encoder->batchBarrier(barrier);

  // Verify barriers are pending (not flushed yet)
  // Actual verification requires instrumentation or mock

  // Draw should trigger flush
  encoder->draw(3, 1, 0, 0);

  // Verify barriers were flushed (requires instrumentation)
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions must pass (batching is transparent optimization)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
./build/Debug/RenderSessions.exe --session=MRTSession
```

**Modifications Allowed:**
- ✅ None required - batching should be transparent
- ❌ **DO NOT modify session logic**

### Performance Verification:

```bash
# Benchmark command encoding with barrier-heavy workload
# Expected: 5-10% reduction in command list encoding time
```

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (existing + new batching test)
- [ ] Barrier batching infrastructure added to RenderCommandEncoder
- [ ] `batchBarrier()` and `flushBarriers()` methods implemented
- [ ] Barriers flushed before draw/dispatch calls
- [ ] Auto-flush at 64 barriers prevents unbounded growth
- [ ] All render sessions pass without visual changes
- [ ] Performance measurement shows reduced barrier API call count
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. All render sessions produce identical output (batching is transparent)
2. Performance profiling shows reduced ResourceBarrier call count
3. No visual regressions in complex scenes

**Post in chat:**
```
G-001 Fix Complete - Ready for Review

Barrier Batching Implementation:
- Infrastructure: Added pendingBarriers_ queue to RenderCommandEncoder
- Batching: batchBarrier() accumulates, flushBarriers() submits
- Auto-flush: 64-barrier limit prevents unbounded growth
- Flush points: Before draw/dispatch calls

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Render sessions: PASS (no visual changes)
- Performance: Barrier call count reduced by ~XX%

Awaiting confirmation to proceed.
```

---

## Related Issues

### Related:
- **G-003** - Descriptor heap fragmentation (batching principle applies to descriptors too)
- **COD-003** - Texture state management (correct barrier submission is critical)
- **H-009** - Shader model support (complex shaders may have more barriers)

---

## Implementation Priority

**Priority:** P1 - Medium (GPU Performance)
**Estimated Effort:** 4-5 hours
**Risk:** Low (Batching is transparent optimization; flush points ensure correctness)
**Impact:** 5-10% command encoding performance improvement in barrier-heavy scenarios

**Notes:**
- Start with encoder-level batching infrastructure (Steps 2-4)
- Step 1 (generateMipmap) is low-value since barriers straddle operations
- Step 5 (Texture API change) is future enhancement, not required for initial implementation
- Focus on correctness: Ensure barriers are flushed before GPU work

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#best-practices
- https://learn.microsoft.com/windows/win32/direct3d12/user-mode-heap-synchronization
- https://developer.nvidia.com/blog/dx12-dos-and-donts/
- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3DX12.h
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
