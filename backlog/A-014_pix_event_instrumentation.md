# A-014: Missing PIX Event Markers for GPU Profiling

**Severity:** Low
**Category:** Debugging & Profiling
**Status:** Open
**Related Issues:** None
**Priority:** P2-Low

---

## Problem Statement

The D3D12 command encoders (RenderCommandEncoder, ComputeCommandEncoder) do not emit PIX event markers for GPU profiling. PIX (Performance Analyzer for DirectX) is the standard Microsoft profiling tool for D3D12 applications, and event markers provide critical visibility into GPU workload timing, memory access patterns, and bottlenecks.

**Impact Analysis:**
- **Profiling Visibility:** Developers cannot profile GPU performance using PIX without manually instrumenting external code
- **Debugging Difficulty:** Long-running GPU operations cannot be identified and analyzed
- **Performance Optimization:** Cannot identify bottleneck operations (draws, dispatches, transfers) in complex scenes
- **Production Diagnostics:** Difficult to diagnose performance issues reported by end users

**The Limitation:**
- PIX events are optional instrumentation but standard practice in professional graphics applications
- Lack of markers makes profiling IGL-based applications significantly harder
- Event hierarchies (Begin/End pairs) enable detailed GPU timeline analysis

---

## Root Cause Analysis

### Current Implementation (RenderCommandEncoder.cpp):

```cpp
void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  // ... validation and setup ...

  // Draw directly - NO PIX event markers!
  commandList_->DrawInstanced(
      static_cast<UINT>(vertexCount),
      instanceCount,
      firstVertex,
      firstInstance);
}

void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t firstInstance) {
  // ... validation and setup ...

  // Draw directly - NO PIX event markers!
  commandList_->DrawIndexedInstanced(
      static_cast<UINT>(indexCount),
      instanceCount,
      firstIndex,
      vertexOffset,
      firstInstance);
}
```

**ComputeCommandEncoder.cpp - Similar Issue:**

```cpp
void ComputeCommandEncoder::dispatchThreads(const Dispatch& dispatch) {
  // ... validation and setup ...

  // Dispatch directly - NO PIX event markers!
  commandList_->Dispatch(dispatch.threadGroupCountX,
                          dispatch.threadGroupCountY,
                          dispatch.threadGroupCountZ);
}
```

### Why This Is Wrong:

1. **No Event Markers:** Commands execute without any PIX instrumentation
2. **No Hierarchy:** Cannot group related operations (e.g., "Render Pass", "Bloom Extraction")
3. **No Timing Context:** GPU timeline shows raw GPU time without semantic context
4. **Missing Standard Practice:** Professional graphics applications always emit PIX events

---

## Official Documentation References

1. **PIX Overview and Usage**:
   - https://devblogs.microsoft.com/pix/documentation/
   - Key guidance: "PIX events provide GPU timeline instrumentation for performance analysis"

2. **PIX Event API (PIXEvents.h)**:
   - https://github.com/microsoft/PIX/blob/master/PIXEvents/WinPixEventRuntime/pix3.h
   - Event marking functions: `PIXBeginEvent()`, `PIXEndEvent()`, `PIXSetMarker()`

3. **D3D12 Command List Event Recording**:
   - https://learn.microsoft.com/windows/win32/direct3d12/user-defined-work-in-the-gpu-timeline
   - Guidance: "Use BeginEvent and EndEvent for GPU timeline annotations"

4. **PIX Best Practices**:
   - https://devblogs.microsoft.com/pix/latest-features-and-best-practices/
   - Recommendations for event naming, hierarchy, and color coding

5. **DirectX-Graphics-Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/search?q=BeginEvent
   - Examples of PIX event usage in D3D12 samples

---

## Code Location Strategy

### Files to Modify:

1. **RenderCommandEncoder.h** (add event context member):
   - Search for: Private member variables section
   - Context: Add optional debug name tracking
   - Action: Add event naming support infrastructure

2. **RenderCommandEncoder.cpp** (emit PIX events in draw calls):
   - Search for: `drawIndexed()` method
   - Context: Draw call execution
   - Action: Wrap with PIX event markers

3. **RenderCommandEncoder.cpp** (emit PIX events in passes):
   - Search for: `pushDebugMarker()` or similar
   - Context: Debug marker handling
   - Action: Emit PIX events for marker hierarchies

4. **ComputeCommandEncoder.cpp** (emit PIX events in dispatch):
   - Search for: `dispatchThreads()` method
   - Context: Compute dispatch execution
   - Action: Wrap with PIX event markers

5. **CommandBuffer.cpp** (emit PIX events in submission):
   - Search for: `ExecuteCommandLists` or `submit()` method
   - Context: Command list submission
   - Action: Optional top-level event for full command buffer

---

## Detection Strategy

### How to Reproduce:

1. Build application with PIX SDK headers available
2. Run application under PIX profiler:
   ```
   "C:\Program Files\WindowsApps\Microsoft.PIX*\PIXLauncher.exe"
   ```
3. Capture GPU timeline
4. Observe GPU timeline lacks semantic event markers

### Expected Behavior (After Fix):

PIX GPU timeline shows:
```
[GPU Timeline]
├─ Draw 0 (render pass 0)
│  ├─ RenderPipelineState bind
│  ├─ DrawIndexed (45000 indices)
│  └─ Duration: 0.521ms
├─ Transition (intermediate)
└─ Draw 1 (render pass 1)
   ├─ RenderPipelineState bind
   ├─ DrawIndexed (30000 indices)
   └─ Duration: 0.389ms
```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Include PIX Runtime Headers

**File:** `src/igl/d3d12/RenderCommandEncoder.h`

**Locate:** Top of file, after existing includes

**Add Headers:**
```cpp
// A-014: PIX event instrumentation support
#if defined(_DEBUG) || defined(ENABLE_PIX)
  #include "pix3.h"
  #pragma comment(lib, "WinPixEventRuntime.lib")
  #define PIX_ENABLED 1
#else
  #define PIX_ENABLED 0
#endif
```

**Rationale:** Conditionally include PIX runtime headers. PIX can be enabled in both Debug and Release builds.

---

#### Step 2: Add Event Context to Encoder

**File:** `src/igl/d3d12/RenderCommandEncoder.h`

**Locate:** Private member variables section

**Current (PROBLEM):**
```cpp
class RenderCommandEncoder : public IRenderCommandEncoder {
 private:
  CommandBuffer& commandBuffer_;
  ID3D12GraphicsCommandList* commandList_ = nullptr;
  // ... other members ...

  // No event context!
};
```

**Fixed (SOLUTION):**
```cpp
class RenderCommandEncoder : public IRenderCommandEncoder {
 private:
  CommandBuffer& commandBuffer_;
  ID3D12GraphicsCommandList* commandList_ = nullptr;
  // ... other members ...

  // A-014: PIX event instrumentation
  uint32_t debugEventDepth_ = 0;
  static constexpr uint32_t kMaxEventDepth = 32; // Guard against deep nesting

 public:
  // Helper to emit named event markers
  void emitPixEvent(const char* eventName, uint64_t color = 0xFF00FF00);
};
```

**Rationale:** Tracks event nesting depth and provides helper method for event emission.

---

#### Step 3: Implement PIX Event Helpers

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Locate:** End of implementation, before namespace close

**Add Helper Methods:**
```cpp
void RenderCommandEncoder::emitPixEvent(const char* eventName, uint64_t color) {
  // A-014: Emit PIX event marker if enabled
  if (!commandList_ || !eventName) {
    return;
  }

#if PIX_ENABLED
  PIXSetMarker(commandList_, color, eventName);
#endif
}

// Helper macro for scoped events (Begin/End pairs)
namespace {
struct PixEventScope {
  ID3D12GraphicsCommandList* cmdList = nullptr;
  uint64_t color = 0;

  PixEventScope(ID3D12GraphicsCommandList* cmd, uint64_t c, const char* name)
      : cmdList(cmd), color(c) {
#if PIX_ENABLED
    if (cmdList && name) {
      PIXBeginEvent(cmdList, color, name);
    }
#endif
  }

  ~PixEventScope() {
#if PIX_ENABLED
    if (cmdList) {
      PIXEndEvent(cmdList);
    }
#endif
  }

  // Deleted copy operations
  PixEventScope(const PixEventScope&) = delete;
  PixEventScope& operator=(const PixEventScope&) = delete;
};
}  // namespace
```

**Rationale:** Scoped event wrapper ensures Begin/End pairs are always balanced, even with early returns.

---

#### Step 4: Emit Events in Draw Calls

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Locate:** `draw()` method

**Current (PROBLEM):**
```cpp
void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  // ... validation ...

  commandList_->DrawInstanced(
      static_cast<UINT>(vertexCount),
      instanceCount,
      firstVertex,
      firstInstance);
}
```

**Fixed (SOLUTION):**
```cpp
void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  // ... validation ...

  // A-014: Emit PIX event for draw call
  PixEventScope drawEvent(commandList_, 0xFF00FF00, "Draw");  // Green

  commandList_->DrawInstanced(
      static_cast<UINT>(vertexCount),
      instanceCount,
      firstVertex,
      firstInstance);
}
```

**Apply Same Pattern to `drawIndexed()`:**
```cpp
void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t firstInstance) {
  // ... validation ...

  // A-014: Emit PIX event for indexed draw
  PixEventScope drawEvent(commandList_, 0xFF00FF00, "DrawIndexed");  // Green

  commandList_->DrawIndexedInstanced(
      static_cast<UINT>(indexCount),
      instanceCount,
      firstIndex,
      vertexOffset,
      firstInstance);
}
```

**Color Coding Convention:**
- Green (0xFF00FF00) - Draw calls
- Blue (0xFFFF0000) - Resource transitions
- Orange (0xFF00FFFF) - Compute dispatch
- Red (0xFF0000FF) - Error/warning states

**Rationale:** Scoped events automatically balance Begin/End pairs. Color codes help visual timeline analysis.

---

#### Step 5: Emit Events in Render Passes

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Locate:** `beginRenderPass()` method

**Current (PROBLEM):**
```cpp
void RenderCommandEncoder::beginRenderPass(
    const std::shared_ptr<IFramebuffer>& framebuffer,
    size_t colorAttachmentCount,
    const AttachmentDesc* colorAttachments,
    const AttachmentDesc* depthAttachment) {
  // ... setup ...
  // No event marking!
}
```

**Fixed (SOLUTION):**
```cpp
void RenderCommandEncoder::beginRenderPass(
    const std::shared_ptr<IFramebuffer>& framebuffer,
    size_t colorAttachmentCount,
    const AttachmentDesc* colorAttachments,
    const AttachmentDesc* depthAttachment) {
  // ... setup ...

  // A-014: Emit PIX event for render pass
  char passName[128] = {0};
  snprintf(passName, sizeof(passName), "RenderPass (%zu colors)", colorAttachmentCount);

#if PIX_ENABLED
  PIXBeginEvent(commandList_, 0xFFFFFF00, passName);  // Yellow for passes
#endif
  ++debugEventDepth_;
}
```

**Locate:** `endRenderPass()` method

**Fixed (SOLUTION):**
```cpp
void RenderCommandEncoder::endRenderPass() {
  // ... cleanup ...

  // A-014: Emit PIX event end
#if PIX_ENABLED
  if (debugEventDepth_ > 0) {
    PIXEndEvent(commandList_);
    --debugEventDepth_;
  }
#endif
}
```

**Rationale:** Render passes are logical grouping; marking them enables profiler to show per-pass timing.

---

#### Step 6: Emit Events in Compute Dispatch

**File:** `src/igl/d3d12/ComputeCommandEncoder.cpp`

**Locate:** `dispatchThreads()` method

**Current (PROBLEM):**
```cpp
void ComputeCommandEncoder::dispatchThreads(const Dispatch& dispatch) {
  // ... validation and setup ...

  commandList_->Dispatch(dispatch.threadGroupCountX,
                          dispatch.threadGroupCountY,
                          dispatch.threadGroupCountZ);
}
```

**Fixed (SOLUTION):**
```cpp
void ComputeCommandEncoder::dispatchThreads(const Dispatch& dispatch) {
  // ... validation and setup ...

  // A-014: Emit PIX event for compute dispatch
  char dispatchName[128] = {0};
  snprintf(dispatchName, sizeof(dispatchName),
           "Dispatch (%u,%u,%u)",
           dispatch.threadGroupCountX, dispatch.threadGroupCountY, dispatch.threadGroupCountZ);

  PixEventScope dispatchEvent(commandList_, 0xFFFF0000, dispatchName);  // Orange

  commandList_->Dispatch(dispatch.threadGroupCountX,
                          dispatch.threadGroupCountY,
                          dispatch.threadGroupCountZ);
}
```

**Rationale:** Compute dispatches benefit from detailed timing analysis, especially for large thread group counts.

---

#### Step 7: Optional - Add Custom Event Names

**File:** `src/igl/d3d12/RenderCommandEncoder.h`

**Add Optional Method:**
```cpp
// A-014: Custom PIX event for user-defined markers
void pushDebugEvent(const std::string& name, uint64_t color = 0xFFFFFFFF) {
  if (debugEventDepth_ >= kMaxEventDepth) {
    IGL_LOG_ERROR("RenderCommandEncoder: Debug event nesting too deep\n");
    return;
  }

#if PIX_ENABLED
  PIXBeginEvent(commandList_, color, name.c_str());
#endif
  ++debugEventDepth_;
}

void popDebugEvent() {
  if (debugEventDepth_ == 0) {
    IGL_LOG_ERROR("RenderCommandEncoder: Debug event pop with no matching push\n");
    return;
  }

#if PIX_ENABLED
  PIXEndEvent(commandList_);
#endif
  --debugEventDepth_;
}
```

**Rationale:** Allows application code to add custom event hierarchies for application-level profiling.

---

## Testing Requirements

### Unit Tests (OPTIONAL - PIX instrumentation is non-functional):

```bash
# Run all D3D12 tests (events should not affect behavior)
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Rendering tests (ensure events don't change output)
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Render*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Draw*"
```

**Test Modifications Allowed:**
- ✅ No modifications required - events are instrumentation only
- ❌ **DO NOT modify test logic**

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions must pass (events should be transparent)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
./build/Debug/RenderSessions.exe --session=MRTSession
```

**Modifications Allowed:**
- ✅ None required - events are transparent
- ❌ **DO NOT modify session logic**

### PIX Profiling Verification:

1. **Capture with PIX Profiler:**
   ```
   1. Launch PIX from: C:\Program Files\WindowsApps\Microsoft.PIX*\
   2. Create new capture
   3. Select test application executable
   4. Capture for ~5 seconds
   5. Export GPU timeline
   ```

2. **Verify Event Markers:**
   - [ ] GPU timeline shows "Draw", "DrawIndexed" events
   - [ ] Render passes show as event hierarchies (Begin/End pairs)
   - [ ] Compute dispatches show with thread group counts
   - [ ] Events have correct color coding
   - [ ] Event names match source code (e.g., "Draw", "RenderPass")

3. **Benchmark (Optional):**
   ```bash
   # PIX events have minimal overhead (~1-2%)
   # Compare frame time before/after instrumentation
   ```

---

## Definition of Done

### Completion Criteria:

- [ ] PIX runtime headers included (pix3.h, WinPixEventRuntime.lib)
- [ ] PixEventScope helper implemented (Begin/End balancing)
- [ ] Draw calls emit green (0xFF00FF00) events
- [ ] Indexed draws emit green (0xFF00FF00) events
- [ ] Render passes emit yellow (0xFFFFFF00) events with hierarchy
- [ ] Compute dispatch emits orange (0xFFFF0000) events
- [ ] Optional custom event methods implemented
- [ ] All unit tests pass
- [ ] All render sessions pass without visual changes
- [ ] PIX profiler shows event markers in GPU timeline
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. PIX profiler shows event markers in GPU timeline
2. Event names are correct (Draw, DrawIndexed, RenderPass, Dispatch)
3. All render sessions produce identical output
4. Event overhead is acceptable (<2%)

**Post in chat:**
```
A-014 Fix Complete - Ready for Review

PIX Event Instrumentation:
- Infrastructure: PixEventScope helper for balanced Begin/End pairs
- Draw Events: Green markers for draw/drawIndexed calls
- Pass Events: Yellow markers for render pass Begin/End hierarchy
- Dispatch Events: Orange markers for compute dispatch with thread counts
- Custom Events: Optional pushDebugEvent/popDebugEvent for user code

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Render sessions: PASS (no visual changes)
- PIX capture: Shows event markers in GPU timeline
- Event overhead: ~1-2% (acceptable)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **None** - This is independent instrumentation feature

---

## Implementation Priority

**Priority:** P2-Low (Debugging & Profiling)
**Estimated Effort:** 2-3 hours
**Risk:** Very Low (Events are instrumentation only; no functional impact)
**Impact:** Enables GPU profiling analysis with PIX; improves debugging visibility

**Notes:**
- PIX events are optional instrumentation with minimal overhead (~1-2%)
- Can be enabled/disabled at compile time via PIX_ENABLED macro
- No functional impact on graphics output or performance
- Professional practice for shipping graphics applications
- Helps diagnose performance issues in production

---

## References

- https://devblogs.microsoft.com/pix/documentation/
- https://github.com/microsoft/PIX/blob/master/PIXEvents/WinPixEventRuntime/pix3.h
- https://learn.microsoft.com/windows/win32/direct3d12/user-defined-work-in-the-gpu-timeline
- https://devblogs.microsoft.com/pix/latest-features-and-best-practices/
- https://github.com/microsoft/DirectX-Graphics-Samples/search?q=BeginEvent

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
