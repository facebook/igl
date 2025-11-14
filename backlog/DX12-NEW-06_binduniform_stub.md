# DX12-NEW-06: bindUniform Stubbed (Not Implemented)

**Severity:** Medium
**Category:** Shaders & Tooling / API Parity
**Status:** Open
**Related Issues:** DX12-NEW-04 (Push constant limits), C-003 (Root signature cost)

---

## Problem Statement

The `bindUniform` API is **completely stubbed** in both `RenderCommandEncoder` and `ComputeCommandEncoder` on the D3D12 backend. Calls to this legacy API simply log "not supported" and discard the uniform data, while Vulkan and OpenGL backends honor the API contract. This creates a silent failure mode for cross-platform applications that use legacy uniform bindings.

**The Impact:**
- Cross-platform code using `bindUniform` works on Vulkan/GL, silently fails on D3D12
- Shader uniform inputs are missing, causing incorrect rendering (black screens, wrong colors)
- No error propagation - application doesn't know uniforms were dropped
- API contract violation (IGL promises uniform binding support)
- Forces applications to use backend-specific workarounds

**Example Failure:**
```cpp
// Cross-platform rendering code
auto encoder = commandBuffer->createRenderCommandEncoder(...);

// Bind uniforms using legacy API
struct Uniforms {
  glm::vec4 color;
  glm::mat4 mvp;
};
Uniforms uniforms = { {1, 0, 0, 1}, calculateMVP() };

encoder->bindUniform(UniformDesc{...}, &uniforms);  // ❌ Silently dropped on D3D12!
encoder->draw(...);

// Vulkan/GL: Renders red cube with correct transform
// D3D12: Renders black cube (uniforms are zeros)
```

---

## Root Cause Analysis

### Current Implementation (Stubbed):

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp:1026`

```cpp
void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc,
                                       const void* data) {
  // ❌ STUB - Does nothing!
  IGL_LOG_WARN("RenderCommandEncoder::bindUniform: Not supported on D3D12 backend\n");
  // Uniform data discarded
}
```

**File:** `src/igl/d3d12/ComputeCommandEncoder.cpp:448-452`

```cpp
void ComputeCommandEncoder::bindUniform(const UniformDesc& uniformDesc,
                                        const void* data) {
  // ❌ STUB - Does nothing!
  IGL_LOG_WARN("ComputeCommandEncoder::bindUniform: Not supported\n");
  // Uniform data discarded
}
```

### Vulkan Implementation (Working):

**File:** `src/igl/vulkan/RenderCommandEncoder.cpp` (approximate)

```cpp
void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc,
                                       const void* data) {
  // Upload to transient constant buffer
  auto& ctx = commandBuffer_.getContext();
  auto cbv = ctx.allocateTransientConstantBuffer(uniformDesc.size);

  memcpy(cbv.cpuAddress, data, uniformDesc.size);

  // Bind descriptor
  vkCmdBindDescriptorSets(
    commandList_,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout_,
    uniformDesc.set,
    1,
    &cbv.descriptorSet,
    0,
    nullptr
  );
}
```

### Why This Is Wrong:

**IGL API Contract (CommandEncoder.h):**
```cpp
/// Bind uniform data to the specified index
/// @param uniformDesc Descriptor for the uniform binding
/// @param data Pointer to uniform data
virtual void bindUniform(const UniformDesc& uniformDesc, const void* data) = 0;
```

**The API Contract States:**
- Method is **pure virtual** (must be implemented)
- All backends must support uniform binding
- Cross-platform code relies on this working

**D3D12 Implementation Options:**

1. **Route to Push Constants (Root Constants):**
   - Small uniforms (≤128 bytes) → `SetGraphicsRoot32BitConstants`
   - Fast, no descriptor required
   - Limited size

2. **Route to Transient CBVs:**
   - Larger uniforms → Upload to ring buffer, create CBV descriptor
   - Unlimited size, slower than push constants
   - Matches Vulkan approach

3. **Hybrid Approach:**
   - Small uniforms (≤64 bytes) → Push constants
   - Large uniforms (>64 bytes) → Transient CBVs
   - Optimal performance

---

## Official Documentation References

1. **Root Constants (Small Uniforms)**:
   - [Root Signatures - Root Constants](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures#root-constants)
   - Quote: "Root constants are 32-bit values that appear in shaders as a constant buffer."

2. **Constant Buffer Views (Large Uniforms)**:
   - [Creating Constant Buffer Views](https://learn.microsoft.com/windows/win32/direct3d12/creating-constant-buffer-views)
   - Quote: "Constant buffers must be aligned to 256 bytes."

3. **Dynamic Constant Buffers**:
   - [MiniEngine DynamicUploadHeap](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicUploadHeap.cpp)
   - Pattern: Upload to ring buffer, bind CBV descriptor

4. **Cross-Platform Uniforms**:
   - IGL API design: Uniform binding abstraction over push constants/CBVs/uniform buffers

---

## Code Location Strategy

### Files to Modify:

1. **RenderCommandEncoder.cpp** (`bindUniform` method):
   - Search for: `bindUniform.*not supported`
   - Context: Stubbed implementation
   - Action: Implement via push constants or transient CBVs

2. **ComputeCommandEncoder.cpp** (`bindUniform` method):
   - Search for: `bindUniform.*not supported`
   - Context: Stubbed implementation
   - Action: Same as render encoder

3. **Device.cpp** (Add transient CBV allocation):
   - Search for: Upload ring buffer management
   - Context: Resource upload infrastructure
   - Action: Add `allocateTransientConstantBuffer()` method

4. **DescriptorHeapManager.cpp** (CBV descriptor allocation):
   - Search for: `allocateCbvSrvUavDescriptor`
   - Context: Descriptor allocation
   - Action: Verify can allocate transient CBV descriptors

---

## Detection Strategy

### How to Reproduce the Bug:

1. **Simple Uniform Binding Test**:
   ```cpp
   auto encoder = commandBuffer->createRenderCommandEncoder(...);

   // Bind uniform data
   struct MyUniforms {
     float color[4];  // Red
   };
   MyUniforms uniforms = { {1.0f, 0.0f, 0.0f, 1.0f} };

   UniformDesc desc;
   desc.location = 0;
   desc.size = sizeof(uniforms);
   desc.type = UniformType::Float4;

   encoder->bindUniform(desc, &uniforms);
   encoder->draw(...);

   // D3D12: Warning logged, uniforms NOT uploaded
   // Vulkan: Uniforms uploaded, renders red
   ```

2. **Cross-Platform Parity Test**:
   ```cpp
   // Same scene on Vulkan and D3D12
   renderSceneWithUniforms(vulkanEncoder);  // ✓ Works
   renderSceneWithUniforms(d3d12Encoder);   // ❌ Black (uniforms missing)
   ```

3. **Shader Verification**:
   ```hlsl
   cbuffer Uniforms : register(b0) {
     float4 color;
   };

   float4 main() : SV_Target {
     return color;  // D3D12: (0,0,0,0), Vulkan: (1,0,0,1)
   }
   ```

### Verification After Fix:

1. `bindUniform` no longer logs warning
2. Uniform data appears in shaders
3. Cross-platform parity test passes (D3D12 matches Vulkan)
4. Render sessions using uniforms produce correct output

---

## Fix Guidance

### Step-by-Step Implementation:

#### Option 1: Route Small Uniforms to Push Constants

**Pros:** Fast, no descriptor allocation
**Cons:** Limited size (64-128 bytes)

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Fixed:**
```cpp
void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc,
                                       const void* data) {
  const size_t maxPushConstantSize = 64;  // Current encoder limit

  if (uniformDesc.size <= maxPushConstantSize) {
    // Small uniform → Use push constants (root constants)
    commandList_->SetGraphicsRoot32BitConstants(
      2,                           // Root parameter index for push constants
      uniformDesc.size / 4,        // Num32BitValues
      data,
      uniformDesc.offset / 4       // DestOffsetIn32BitValues
    );

    IGL_LOG_INFO("RenderCommandEncoder::bindUniform: Uploaded %zu bytes via push constants\n",
                 uniformDesc.size);
  } else {
    // Large uniform → Need transient CBV (see Option 2)
    IGL_LOG_ERROR("RenderCommandEncoder::bindUniform: Uniform size %zu exceeds "
                  "push constant limit %zu (transient CBVs not implemented)\n",
                  uniformDesc.size, maxPushConstantSize);
  }
}
```

---

#### Option 2: Route Large Uniforms to Transient CBVs

**Pros:** Unlimited size, matches Vulkan
**Cons:** Requires descriptor allocation, ring buffer upload

**Step 1: Add Transient CBV Allocator**

**File:** `src/igl/d3d12/Device.h`

```cpp
class Device {
 public:
  // ... existing methods ...

  // Allocate transient constant buffer (per-frame, GPU-visible)
  struct TransientCBV {
    void* cpuAddress;              // CPU-writable address
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;  // GPU address
    uint32_t descriptorIndex;      // CBV descriptor index
  };

  TransientCBV allocateTransientConstantBuffer(size_t size);

 private:
  // Ring buffer for transient constant buffer uploads
  std::unique_ptr<UploadRingBuffer> transientCBVRingBuffer_;
};
```

**Step 2: Implement Allocator**

**File:** `src/igl/d3d12/Device.cpp`

```cpp
Device::TransientCBV Device::allocateTransientConstantBuffer(size_t size) {
  // Align to 256 bytes (D3D12 CBV requirement)
  const size_t alignedSize = (size + 255) & ~255;

  // Allocate from ring buffer
  auto allocation = transientCBVRingBuffer_->allocate(alignedSize);

  // Create CBV descriptor
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
  cbvDesc.BufferLocation = allocation.gpuAddress;
  cbvDesc.SizeInBytes = static_cast<UINT>(alignedSize);

  uint32_t descriptorIndex = ctx_->getDescriptorHeapManager()->allocateCbvSrvUavDescriptor();
  auto cpuHandle = ctx_->getDescriptorHeapManager()->getCpuHandle(descriptorIndex);

  device_->CreateConstantBufferView(&cbvDesc, cpuHandle);

  return TransientCBV {
    allocation.cpuAddress,
    allocation.gpuAddress,
    descriptorIndex
  };
}
```

**Step 3: Use in bindUniform**

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

```cpp
void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc,
                                       const void* data) {
  auto& device = commandBuffer_.getDevice();

  // Allocate transient CBV
  auto cbv = device.allocateTransientConstantBuffer(uniformDesc.size);

  // Copy uniform data to GPU-visible memory
  memcpy(cbv.cpuAddress, data, uniformDesc.size);

  // Bind CBV descriptor table
  // (Assumes root signature has CBV descriptor table at index 0)
  auto gpuHandle = ctx_->getDescriptorHeapManager()->getGpuHandle(cbv.descriptorIndex);
  commandList_->SetGraphicsRootDescriptorTable(0, gpuHandle);

  IGL_LOG_INFO("RenderCommandEncoder::bindUniform: Uploaded %zu bytes via transient CBV\n",
               uniformDesc.size);
}
```

---

#### Option 3: Hybrid Approach (Recommended)

**Combine Option 1 and Option 2:**

```cpp
void RenderCommandEncoder::bindUniform(const UniformDesc& uniformDesc,
                                       const void* data) {
  const size_t pushConstantThreshold = 64;

  if (uniformDesc.size <= pushConstantThreshold) {
    // Small uniform → Push constants (fast path)
    commandList_->SetGraphicsRoot32BitConstants(
      2, uniformDesc.size / 4, data, uniformDesc.offset / 4
    );
  } else {
    // Large uniform → Transient CBV (slow path)
    auto& device = commandBuffer_.getDevice();
    auto cbv = device.allocateTransientConstantBuffer(uniformDesc.size);
    memcpy(cbv.cpuAddress, data, uniformDesc.size);

    auto gpuHandle = ctx_->getDescriptorHeapManager()->getGpuHandle(cbv.descriptorIndex);
    commandList_->SetGraphicsRootDescriptorTable(0, gpuHandle);
  }
}
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Specific uniform binding tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Uniform*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*BindUniform*"
```

**Test Modifications Allowed:**
- ✅ Add `bindUniform` tests (small/large uniforms)
- ✅ Add cross-platform parity tests
- ❌ **DO NOT modify cross-platform test logic**

### New Test Cases Required:

```cpp
TEST(D3D12RenderCommandEncoder, BindUniformSmall) {
  auto encoder = createRenderEncoder();

  // Bind small uniform (≤64 bytes) via push constants
  struct SmallUniform {
    float color[4];
    float mvp[16];
  };
  SmallUniform uniform = { {1,0,0,1}, /* identity matrix */ };

  UniformDesc desc;
  desc.size = sizeof(uniform);
  desc.location = 0;

  encoder->bindUniform(desc, &uniform);
  // Should upload via SetGraphicsRoot32BitConstants (no error)
}

TEST(D3D12RenderCommandEncoder, BindUniformLarge) {
  auto encoder = createRenderEncoder();

  // Bind large uniform (>64 bytes) via transient CBV
  struct LargeUniform {
    float data[256];  // 1024 bytes
  };
  LargeUniform uniform = {};

  UniformDesc desc;
  desc.size = sizeof(uniform);
  desc.location = 0;

  encoder->bindUniform(desc, &uniform);
  // Should upload via transient CBV (no error)
}

TEST(D3D12Device, CrossPlatformUniformParity) {
  // Verify D3D12 and Vulkan both support bindUniform
  auto d3d12Encoder = createD3D12RenderEncoder();
  auto vulkanEncoder = createVulkanRenderEncoder();

  struct Uniforms {
    float color[4];
  };
  Uniforms uniforms = { {1, 0, 0, 1} };

  UniformDesc desc;
  desc.size = sizeof(uniforms);
  desc.location = 0;

  // Both should accept uniform binding (no error/warning)
  d3d12Encoder->bindUniform(desc, &uniforms);
  vulkanEncoder->bindUniform(desc, &uniforms);

  // (Verify via shader readback that both received correct data)
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass
./test_all_sessions.bat
```

**Expected Changes:**
- Sessions using legacy uniform bindings now work on D3D12
- Visual output matches Vulkan/GL

**Modifications Allowed:**
- ✅ None required
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] `bindUniform` no longer logs warning on D3D12
- [ ] Small uniforms (≤64 bytes) uploaded via push constants
- [ ] Large uniforms (>64 bytes) uploaded via transient CBVs
- [ ] Cross-platform parity test passes (D3D12 matches Vulkan)
- [ ] Shaders receive correct uniform data

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Uniform binding tests pass (small and large)
2. Cross-platform parity test passes
3. Render sessions show correct visual output

**Post in chat:**
```
DX12-NEW-06 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- bindUniform: Now implemented (push constants + transient CBVs)
- Cross-platform parity: D3D12 matches Vulkan

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- **DX12-NEW-04** (Push constant limits) - Must fix limits before routing uniforms to push constants

### Blocks:
- Legacy uniform-based rendering code
- Cross-platform shader compatibility

### Related:
- **C-003** (Root signature cost) - Transient CBVs affect descriptor table layout

---

## Implementation Priority

**Priority:** P1 - MEDIUM (API Parity Gap)
**Estimated Effort:** 6-8 hours (Option 1+2), 8-10 hours (Option 3 hybrid)
**Risk:** Medium (requires transient CBV infrastructure)
**Impact:** MEDIUM - Fixes cross-platform uniform binding, enables legacy code paths

---

## References

- [Root Signatures - Root Constants](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures#root-constants)
- [Creating Constant Buffer Views](https://learn.microsoft.com/windows/win32/direct3d12/creating-constant-buffer-views)
- [MiniEngine DynamicUploadHeap](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicUploadHeap.cpp)
- [IGL CommandEncoder.h API Contract](../../src/igl/CommandEncoder.h)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
