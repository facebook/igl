# DX12-NEW-01: Descriptor Heap Rebinding on Dynamic Page Allocation

**Severity:** Critical
**Category:** Descriptors & Root Signatures
**Status:** Open
**Related Issues:** C-001 (Descriptor exhaustion returns UINT32_MAX)

---

## Problem Statement

When the descriptor heap grows beyond the initial 1,024 CBV/SRV/UAV descriptors per page, `CommandBuffer::getNextCbvSrvUavDescriptor()` correctly allocates a new shader-visible heap page and updates `currentCbvSrvUavPageIndex`. However, **none of the command encoders rebind the D3D12 command list to the new heap**.

All render and compute encoders continue calling `context.getCbvSrvUavHeap()`, which returns only the **first page** (page 0). Any descriptors allocated on pages 1-15 reference an **unbound heap**, causing the GPU to read garbage data and producing undefined behavior.

This is a **production blocker** because:
- Applications with >1,024 unique textures/buffers per frame crash or render incorrectly
- The bug is silent - no validation errors, just incorrect GPU reads
- Stress tests and complex scenes immediately trigger this condition

---

## Root Cause Analysis

### Current Implementation Flow:

1. **Descriptor Allocation** (`CommandBuffer.cpp:97-154`):
   ```cpp
   Result CommandBuffer::getNextCbvSrvUavDescriptor(uint32_t* outIndex) {
     auto& frameCtx = ctx_.getFrameContexts()[ctx_.getCurrentFrameIndex()];

     if (frameCtx.nextCbvSrvUavDescriptor >= kDescriptorsPerPage) {
       // Allocate new page
       frameCtx.currentCbvSrvUavPageIndex++;
       frameCtx.nextCbvSrvUavDescriptor = 0;

       // BUG: New heap allocated but NOT rebound to command list!
       allocateDescriptorHeapPage(frameCtx);
     }

     *outIndex = frameCtx.nextCbvSrvUavDescriptor++;
     return Result();
   }
   ```

2. **Encoder Heap Binding** (`RenderCommandEncoder.cpp:22-56`):
   ```cpp
   RenderCommandEncoder::RenderCommandEncoder(...) {
     // BUG: Always binds page 0, ignoring currentCbvSrvUavPageIndex
     ID3D12DescriptorHeap* heaps[] = {
       context_.getCbvSrvUavHeap(),  // Returns ONLY page 0!
       context_.getSamplerHeap()
     };
     commandList_->SetDescriptorHeaps(2, heaps);
   }
   ```

3. **Result:** Descriptors written to pages 1-15 use **unbound GPU virtual addresses**, causing:
   - Null texture reads (black/magenta rendering)
   - Access violations (device removal)
   - Race conditions (reading wrong resource data)

### Why This Happens:

The descriptor heap paging system was added to scale beyond 1,024 descriptors, but the encoder initialization was never updated to track which page is active. The `getCbvSrvUavHeap()` accessor is a legacy method that predates multi-page support.

---

## Official Documentation References

1. **Descriptor Heap Binding**:
   - [Binding Descriptor Heaps (Microsoft Learn)](https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps#binding-descriptor-heaps)
   - Quote: "Before you use descriptor tables that reference a descriptor heap, you must set the descriptor heap on the command list using `ID3D12GraphicsCommandList::SetDescriptorHeaps`."

2. **Dynamic Descriptor Indexing**:
   - [Microsoft DirectX-Graphics-Samples: MiniEngine DynamicDescriptorHeap](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicDescriptorHeap.h)
   - MiniEngine rebinds heaps whenever the active page changes

3. **Descriptor Handle Arithmetic**:
   - [Descriptor Handles (Microsoft Learn)](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview#descriptor-handles)
   - GPU handles are only valid for the heap they were created from

---

## Code Location Strategy

### Files to Modify:

1. **CommandBuffer.cpp** (`getNextCbvSrvUavDescriptor` method):
   - Search for: `currentCbvSrvUavPageIndex++`
   - Context: Descriptor allocation logic that increments page index
   - Action: Store the new heap COM pointer for encoder access

2. **D3D12Context.h** (FrameContext structure):
   - Search for: `struct FrameContext` and `currentCbvSrvUavPageIndex`
   - Context: Per-frame descriptor heap management
   - Action: Add `ID3D12DescriptorHeap* activeCbvSrvUavHeap` field

3. **RenderCommandEncoder.cpp** (Constructor):
   - Search for: `SetDescriptorHeaps` call in constructor
   - Context: Encoder initialization that binds heaps
   - Action: Replace `getCbvSrvUavHeap()` with frame-context active heap

4. **ComputeCommandEncoder.cpp** (Constructor):
   - Search for: `SetDescriptorHeaps` call in constructor
   - Context: Same as render encoder
   - Action: Same fix as render encoder

5. **CommandBuffer.cpp** (`begin` method):
   - Search for: `commandList_->SetDescriptorHeaps` in `begin()`
   - Context: Command buffer initialization
   - Action: Ensure active heap is bound at command buffer start

---

## Detection Strategy

### How to Reproduce:

1. **Create a test with >1,024 unique textures**:
   ```cpp
   // Create 1,100 textures to force page allocation
   std::vector<std::shared_ptr<ITexture>> textures;
   for (int i = 0; i < 1100; i++) {
     TextureDesc desc;
     desc.type = TextureType::TwoD;
     desc.width = 256;
     desc.height = 256;
     desc.format = TextureFormat::RGBA_UNorm8;
     textures.push_back(device->createTexture(desc, nullptr));
   }

   // Bind all textures in a single frame
   commandBuffer->begin();
   auto encoder = commandBuffer->createRenderCommandEncoder(...);
   for (int i = 0; i < 1100; i++) {
     encoder->bindTexture(i, textures[i].get());
   }
   encoder->draw(...);
   encoder->endEncoding();
   ```

2. **Expected symptom**: After descriptor 1,024, textures render as black/magenta or device removal occurs

3. **Validation errors** (enable debug layer):
   ```
   D3D12 ERROR: GPU-based validation: Incompatible resource state:
   Resource: 0x..., Subresource: 0, Descriptor heap index: 1025
   Expected state: PIXEL_SHADER_RESOURCE, Actual state: INVALID
   ```

4. **PIX capture**: Descriptor table shows unbound heap references for indices >1,024

### Verification After Fix:

1. Run the >1,024 texture test - all textures should render correctly
2. No validation errors about unbound heaps
3. PIX shows correct descriptor heap binding throughout frame

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Track Active Heap in FrameContext

**File:** `src/igl/d3d12/D3D12Context.h`

**Locate:** `struct FrameContext` definition

**Modify:**
```cpp
struct FrameContext {
  // Existing fields...
  uint32_t currentCbvSrvUavPageIndex = 0;
  uint32_t nextCbvSrvUavDescriptor = 0;

  // NEW: Track the currently active shader-visible heap
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> activeCbvSrvUavHeap;

  // Existing fields...
};
```

**Rationale:** Encoders need direct access to the active heap, not a legacy "first page" accessor.

---

#### Step 2: Store Active Heap When Allocating Pages

**File:** `src/igl/d3d12/CommandBuffer.cpp`

**Locate:** `getNextCbvSrvUavDescriptor` method, specifically where `currentCbvSrvUavPageIndex++` occurs

**Modify:**
```cpp
Result CommandBuffer::getNextCbvSrvUavDescriptor(uint32_t* outIndex) {
  auto& frameCtx = ctx_.getFrameContexts()[ctx_.getCurrentFrameIndex()];

  if (frameCtx.nextCbvSrvUavDescriptor >= kDescriptorsPerPage) {
    frameCtx.currentCbvSrvUavPageIndex++;

    if (frameCtx.currentCbvSrvUavPageIndex >= kMaxHeapPages) {
      return Result(Result::Code::RuntimeError,
                    "Exceeded maximum descriptor heap pages (16)");
    }

    frameCtx.nextCbvSrvUavDescriptor = 0;

    // Allocate new page
    Result allocResult = allocateDescriptorHeapPage(frameCtx);
    if (!allocResult.isOk()) {
      return allocResult;
    }

    // NEW: Store the new heap for encoder rebinding
    frameCtx.activeCbvSrvUavHeap = frameCtx.cbvSrvUavHeapPages[frameCtx.currentCbvSrvUavPageIndex];

    // NEW: Rebind heap on the command list immediately
    if (commandList_.Get()) {
      ID3D12DescriptorHeap* heaps[] = {
        frameCtx.activeCbvSrvUavHeap.Get(),
        frameCtx.samplerHeap.Get()
      };
      commandList_->SetDescriptorHeaps(2, heaps);
    }
  }

  *outIndex = frameCtx.nextCbvSrvUavDescriptor++;
  return Result();
}
```

**Rationale:** Rebind the heap immediately when page changes occur during descriptor allocation.

---

#### Step 3: Initialize Active Heap on Frame Start

**File:** `src/igl/d3d12/CommandBuffer.cpp`

**Locate:** `begin()` method where descriptor heaps are first bound

**Modify:**
```cpp
void CommandBuffer::begin() {
  // Existing initialization...

  auto& frameCtx = ctx_.getFrameContexts()[ctx_.getCurrentFrameIndex()];

  // Initialize active heap to page 0 at frame start
  frameCtx.activeCbvSrvUavHeap = frameCtx.cbvSrvUavHeapPages[0];

  // Bind heaps
  ID3D12DescriptorHeap* heaps[] = {
    frameCtx.activeCbvSrvUavHeap.Get(),  // Use active heap, not legacy accessor
    frameCtx.samplerHeap.Get()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

  // Existing code...
}
```

---

#### Step 4: Update Render Encoder to Use Active Heap

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Locate:** Constructor where `SetDescriptorHeaps` is called

**Modify:**
```cpp
RenderCommandEncoder::RenderCommandEncoder(
    CommandBuffer& commandBuffer,
    const RenderPassDesc& renderPass,
    const FramebufferDesc& framebuffer,
    const Dependencies& dependencies)
    : commandBuffer_(commandBuffer), ... {

  auto& frameCtx = commandBuffer_.getContext().getFrameContexts()[
      commandBuffer_.getContext().getCurrentFrameIndex()];

  // Use active heap from frame context, not legacy accessor
  ID3D12DescriptorHeap* heaps[] = {
    frameCtx.activeCbvSrvUavHeap.Get(),  // FIXED: Use active heap
    frameCtx.samplerHeap.Get()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

  // Existing encoder setup...
}
```

**Rationale:** Encoders must bind the currently active heap, not the hardcoded page 0.

---

#### Step 5: Update Compute Encoder Identically

**File:** `src/igl/d3d12/ComputeCommandEncoder.cpp`

**Locate:** Constructor where `SetDescriptorHeaps` is called

**Apply:** Same fix as Step 4 (use `frameCtx.activeCbvSrvUavHeap.Get()`)

---

#### Step 6: Remove or Mark Legacy Accessor

**File:** `src/igl/d3d12/D3D12Context.h`

**Locate:** `getCbvSrvUavHeap()` method (if it exists as a context-level accessor)

**Action:**
- Add deprecation comment: `// DEPRECATED: Use frameCtx.activeCbvSrvUavHeap instead`
- Or remove if only used in encoders (now fixed)

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 backend unit tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Specific tests to verify:
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Descriptor*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*RenderPass*"
```

**Test Modifications Allowed:**
- ✅ Add new D3D12-specific descriptor heap tests
- ✅ Modify D3D12 backend capability checks in tests
- ❌ **DO NOT modify cross-platform test logic**
- ❌ **DO NOT modify expected results for Vulkan/OpenGL tests**

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all render session tests
./test_all_sessions.bat

# Or individual sessions:
./build/Debug/RenderSessions.exe --session=Textured3DCube
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=MRTSession
```

**Render Session Modifications Allowed:**
- ✅ Update D3D12-specific shaders (HLSL) if needed
- ✅ Modify D3D12 backend initialization in sessions
- ❌ **DO NOT change session render logic (draw calls, geometry, etc.)**
- ❌ **DO NOT modify Vulkan/OpenGL shader equivalents**

### Additional Manual Testing:

1. **Descriptor Stress Test**:
   ```cpp
   // Create test that allocates 2,000+ descriptors in one frame
   // Expected: All textures render correctly, no validation errors
   ```

2. **Multi-Frame Stress Test**:
   ```cpp
   // Run 100 frames with 1,500 descriptors each
   // Expected: No memory leaks, consistent rendering
   ```

3. **PIX Capture Verification**:
   - Capture a frame with >1,024 descriptors
   - Verify `SetDescriptorHeaps` is called when page index changes
   - Verify descriptor table GPU addresses are correct

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (`IGLTests.exe --gtest_filter="*D3D12*"`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] Manual stress test with 2,000+ descriptors renders correctly
- [ ] No D3D12 debug layer validation errors
- [ ] PIX capture shows correct heap bindings
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. All unit tests pass
2. All render sessions pass
3. No regressions in existing functionality

**Post in chat:**
```
DX12-NEW-01 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Stress test (2000 descriptors): PASS
- PIX validation: PASS

Awaiting confirmation to proceed to next task.
```

---

## Related Issues

### Blocked By:
- None (this is a critical path issue)

### Blocks:
- **G-003** (Descriptor heap fragmentation optimization)
- **C-005** (Increase default heap sizes)
- All features requiring >1,024 descriptors per frame

### Related Fixes:
- **C-001** (Descriptor exhaustion error handling) - Both issues affect same code paths
- **DX12-NEW-04** (Push-constant limits) - Root signature capacity must be validated after descriptor fix

---

## Implementation Priority

**Priority:** P0 - CRITICAL
**Estimated Effort:** 4-8 hours
**Risk:** Low (isolated fix, well-defined scope)
**Impact:** HIGH - Unblocks all multi-texture scenarios

---

## References

- [D3D12 Descriptor Heaps Overview](https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps-overview)
- [ID3D12GraphicsCommandList::SetDescriptorHeaps](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setdescriptorheaps)
- [MiniEngine Dynamic Descriptor Heap Implementation](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicDescriptorHeap.cpp)
- [DirectX 12 Programming Guide - Resource Binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
