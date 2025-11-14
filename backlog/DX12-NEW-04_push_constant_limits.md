# DX12-NEW-04: Push-Constant Limit Mismatch

**Severity:** Medium/High
**Category:** Hard-Coded Limits & Cross-API Validation
**Status:** Open
**Related Issues:** C-003 (Root signature cost validation), I-002 (Buffer alignment restrictions)

---

## Problem Statement

The D3D12 backend advertises **256 bytes** of push-constant capacity via `Device::getFeatureLimits()`, but the actual implementation only provides **64-128 bytes** depending on the code path:

- **Reported limit**: 256 bytes (`Device.cpp`)
- **Root signature allocation**: 32 DWORDs = 128 bytes
- **Encoder upload cap**: 64 bytes (render & compute encoders)
- **Vulkan reports**: True hardware limit (typically 128+ bytes)

**The Impact:**
- Applications allocate push-constant buffers based on reported 256-byte limit
- Runtime uploads silently truncate to 64 bytes
- Cross-platform code sees inconsistent limits between D3D12 (256) and Vulkan (128+)
- Shader uniform data corruption when apps exceed actual 64-byte upload limit
- Violates principle of least surprise (API contract mismatch)

**Example Failure:**
```cpp
// App code based on getFeatureLimits()
struct PushConstants {
  mat4x4 mvp;        // 64 bytes
  vec4 color;        // 16 bytes
  vec4 params[10];   // 160 bytes
  // Total: 240 bytes (within reported 256-byte limit)
};

device->getFeatureLimits().pushConstantsMaxSize; // Returns 256 ✓
encoder->bindBytes(..., 240);                     // Silently clamped to 64! ❌
// Result: Only MVP matrix uploaded, rest are zeros
```

---

## Root Cause Analysis

### Current Implementation (Inconsistent Limits):

**1. Device Reporting (`Device.cpp:2228-2258`):**
```cpp
DeviceFeatureLimits Device::getFeatureLimits() const {
  DeviceFeatureLimits limits;
  // ...
  limits.pushConstantsMaxSize = 256;  // ❌ Over-reported!
  return limits;
}
```

**2. Root Signature Allocation (`Device.cpp` - Graphics/Compute Pipelines):**
```cpp
// Root signature only allocates 32 DWORDs = 128 bytes
D3D12_ROOT_PARAMETER rootParams[5];
// ...
rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
rootParams[2].Constants.Num32BitValues = 32;  // ❌ Only 128 bytes!
rootParams[2].Constants.ShaderRegister = 0;   // register(b0)
```

**3. Render Encoder Upload (`RenderCommandEncoder.cpp:742-771`):**
```cpp
void RenderCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  const size_t maxBytes = 64;  // ❌ Most restrictive limit!
  const size_t safeLength = std::min(length, maxBytes);

  // Silently truncates to 64 bytes
  commandList_->SetGraphicsRoot32BitConstants(
    2,                           // Root parameter index
    safeLength / 4,              // Num32BitValuesToSet (16 DWORDs max)
    data,
    0                            // DestOffsetIn32BitValues
  );
}
```

**4. Compute Encoder Upload (`ComputeCommandEncoder.cpp:229-249`):**
```cpp
void ComputeCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  const size_t maxBytes = 64;  // ❌ Same 64-byte cap
  const size_t safeLength = std::min(length, maxBytes);

  commandList_->SetComputeRoot32BitConstants(2, safeLength / 4, data, 0);
}
```

**5. Vulkan Comparison (`vulkan/Device.cpp:731-743`):**
```cpp
limits.pushConstantsMaxSize = physicalDeviceProperties_.limits.maxPushConstantsSize;
// Vulkan reports TRUE hardware limit (typically 128-256 bytes)
```

### Why This Is Wrong:

**Microsoft's Guidance on Root Constants:**
> "Root constants are the most efficient way to set constant data that changes frequently. Each root constant is a 32-bit value. Applications can have up to 64 DWORDs of root constants (spread across multiple parameters)." - [Root Signatures - Root Constants](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures#root-constants)

**The Three-Way Mismatch:**
1. **Advertised**: 256 bytes (64 DWORDs) - technically within spec, but never allocated
2. **Allocated**: 128 bytes (32 DWORDs) - root signature parameter size
3. **Actually Used**: 64 bytes (16 DWORDs) - encoder upload limit

**Cross-Platform Contract Violation:**
- Vulkan backend reports **actual hardware limit** (honest API contract)
- D3D12 backend reports **aspirational limit** (misleading)
- Applications cannot write portable code without backend-specific workarounds

---

## Official Documentation References

1. **Root Constants Specification**:
   - [Root Signatures - Root Constants](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures#root-constants)
   - Quote: "Root constants are 32-bit values that appear in shaders as constant buffer. Each root constant is a 32-bit value (DWORD). Applications can have up to 64 DWORDs total across all root parameters."

2. **Root Signature Cost Limits**:
   - [Root Signature Limits](https://learn.microsoft.com/windows/win32/direct3d12/root-signature-limits)
   - Quote: "Root signatures are limited to 64 DWORDs total. Root constants cost 1 DWORD per 32-bit value."

3. **Vulkan Push Constants**:
   - [VkPhysicalDeviceLimits::maxPushConstantsSize](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html)
   - Quote: "maxPushConstantsSize is the maximum size, in bytes, of the pool of push constant memory. Must be at least 128 bytes."

4. **Metal Push Constants Equivalent**:
   - `setVertexBytes` / `setFragmentBytes` limited by `maxFragmentInputComponents` (4096 bytes typical)

---

## Code Location Strategy

### Files to Modify:

1. **Device.cpp** (`getFeatureLimits` method):
   - Search for: `pushConstantsMaxSize = 256`
   - Context: Feature limits reporting
   - Action: Change to match actual implementation (128 bytes conservative, 64 bytes strict)

2. **Device.cpp** (Graphics pipeline root signature):
   - Search for: `D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS` in graphics pipeline creation
   - Context: `Num32BitValues = 32`
   - Action: Document actual allocation or increase to 64 DWORDs (256 bytes)

3. **Device.cpp** (Compute pipeline root signature):
   - Search for: `D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS` in compute pipeline creation
   - Context: Compute root signature
   - Action: Match graphics pipeline allocation

4. **RenderCommandEncoder.cpp** (`bindBytes` method):
   - Search for: `const size_t maxBytes = 64`
   - Context: Graphics root constants upload
   - Action: Increase to match root signature allocation or document restriction

5. **ComputeCommandEncoder.cpp** (`bindBytes` method):
   - Search for: `const size_t maxBytes = 64`
   - Context: Compute root constants upload
   - Action: Increase to match root signature allocation

---

## Detection Strategy

### How to Reproduce the Bug:

1. **Query Limit Test**:
   ```cpp
   auto device = createD3D12Device();
   auto limits = device->getFeatureLimits();

   printf("Reported push constant size: %zu bytes\n", limits.pushConstantsMaxSize);
   // Output: 256 bytes
   ```

2. **Upload Truncation Test**:
   ```cpp
   // Create render encoder
   auto encoder = commandBuffer->createRenderCommandEncoder();

   // Try to upload 128 bytes (within reported limit)
   uint32_t data[32];  // 128 bytes
   for (int i = 0; i < 32; i++) data[i] = i;

   encoder->bindBytes(0, data, sizeof(data));
   // Only first 16 DWORDs (64 bytes) actually uploaded!

   // Shader receives: 0, 1, 2, ..., 15, 0, 0, 0, ...
   ```

3. **Shader Verification**:
   ```hlsl
   cbuffer PushConstants : register(b0) {
     uint4 data[8];  // 128 bytes
   };

   float4 main() : SV_Target {
     // data[0-3] have values (64 bytes)
     // data[4-7] are zeros (truncated)
     return float4(data[4].x, 0, 0, 1);  // Returns black instead of data!
   }
   ```

### Verification After Fix:

1. Reported limit matches actual encoder upload capacity
2. 128-byte uploads work correctly (32 DWORDs)
3. D3D12 and Vulkan backends report consistent limits
4. Shaders receive all push-constant data

---

## Fix Guidance

### Step-by-Step Implementation:

#### Option 1: Conservative Fix (Report Actual Limit)

**Pros:** Minimal code changes, honest API contract
**Cons:** Reduces advertised capability

**Step 1: Fix Feature Limits Reporting**

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `getFeatureLimits` method

**Current:**
```cpp
DeviceFeatureLimits Device::getFeatureLimits() const {
  DeviceFeatureLimits limits;
  // ...
  limits.pushConstantsMaxSize = 256;  // ❌ Over-reported
  return limits;
}
```

**Fixed:**
```cpp
DeviceFeatureLimits Device::getFeatureLimits() const {
  DeviceFeatureLimits limits;
  // ...
  // Report actual encoder upload limit (conservative)
  limits.pushConstantsMaxSize = 64;  // ✓ Matches RenderCommandEncoder::bindBytes

  // OR report root signature allocation (less conservative):
  // limits.pushConstantsMaxSize = 128;  // ✓ Matches root parameter allocation

  return limits;
}
```

**Step 2: Document Encoder Limits**

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Add comment explaining restriction:**
```cpp
void RenderCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  // D3D12 root constants: currently limited to 64 bytes for compatibility
  // Root signature allocates 32 DWORDs (128 bytes), but we conservatively
  // cap uploads to 16 DWORDs to leave room for future expansion
  const size_t maxBytes = 64;
  const size_t safeLength = std::min(length, maxBytes);

  if (length > maxBytes) {
    IGL_LOG_WARN("RenderCommandEncoder::bindBytes: Requested %zu bytes, "
                 "clamped to %zu (push constant limit)\n", length, maxBytes);
  }

  commandList_->SetGraphicsRoot32BitConstants(2, safeLength / 4, data, 0);
}
```

---

#### Option 2: Increase Encoder Limits (Match Root Signature)

**Pros:** Better utilization, matches spec
**Cons:** More testing required

**Step 1: Increase Encoder Caps to 128 Bytes**

**File:** `src/igl/d3d12/RenderCommandEncoder.cpp`

**Current:**
```cpp
void RenderCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  const size_t maxBytes = 64;  // ❌ Too conservative
  const size_t safeLength = std::min(length, maxBytes);

  commandList_->SetGraphicsRoot32BitConstants(2, safeLength / 4, data, 0);
}
```

**Fixed:**
```cpp
void RenderCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  // Match root signature allocation: 32 DWORDs = 128 bytes
  const size_t maxBytes = 128;
  const size_t safeLength = std::min(length, maxBytes);

  if (length > maxBytes) {
    IGL_LOG_ERROR("RenderCommandEncoder::bindBytes: Requested %zu bytes exceeds "
                  "root constant limit (%zu bytes)\n", length, maxBytes);
  }

  commandList_->SetGraphicsRoot32BitConstants(
    2,                    // Root parameter index (push constants)
    safeLength / 4,       // Num32BitValuesToSet (up to 32 DWORDs)
    data,
    0                     // DestOffsetIn32BitValues
  );
}
```

**Step 2: Update ComputeCommandEncoder**

**File:** `src/igl/d3d12/ComputeCommandEncoder.cpp`

**Apply same change:**
```cpp
void ComputeCommandEncoder::bindBytes(size_t index, const void* data, size_t length) {
  const size_t maxBytes = 128;  // ✓ Match render encoder
  const size_t safeLength = std::min(length, maxBytes);

  commandList_->SetComputeRoot32BitConstants(2, safeLength / 4, data, 0);
}
```

**Step 3: Update Feature Limits**

**File:** `src/igl/d3d12/Device.cpp`

```cpp
limits.pushConstantsMaxSize = 128;  // ✓ Now accurate
```

---

#### Option 3: Full Spec Implementation (256 Bytes)

**Pros:** Maximizes capability, matches advertised limit
**Cons:** Requires root signature redesign, affects root signature cost budget

**Step 1: Increase Root Signature Allocation**

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Graphics pipeline root signature creation

**Current:**
```cpp
D3D12_ROOT_PARAMETER rootParams[5];
// ...
rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
rootParams[2].Constants.Num32BitValues = 32;  // 128 bytes
```

**Fixed:**
```cpp
D3D12_ROOT_PARAMETER rootParams[5];
// ...
rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
rootParams[2].Constants.Num32BitValues = 64;  // ✓ 256 bytes (full spec)
rootParams[2].Constants.ShaderRegister = 0;
rootParams[2].Constants.RegisterSpace = 0;
rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
```

**Step 2: Validate Root Signature Cost**

**Add validation (see C-003 task):**
```cpp
// Calculate root signature cost
// Each root constant = 1 DWORD
// Must not exceed 64 DWORDs total across ALL parameters
uint32_t totalCost = 0;
totalCost += 64;  // Push constants (64 DWORDs)
totalCost += 2;   // Descriptor tables (2 DWORDs each)
// ... add other parameters

if (totalCost > 64) {
  IGL_LOG_ERROR("Root signature exceeds 64 DWORD limit: %u DWORDs\n", totalCost);
  return nullptr;
}
```

**Step 3: Update Encoders**

**Increase encoder limits to 256 bytes (as in Option 2, but with 64 DWORDs)**

---

### Recommended Approach:

**Start with Option 1** (report 64 bytes) to fix the immediate API contract violation. Then **upgrade to Option 2** (128 bytes) after testing. **Option 3** requires careful root signature cost analysis (see C-003 task).

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Specific push constant tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*PushConstant*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*BindBytes*"
```

**Test Modifications Allowed:**
- ✅ Add push-constant upload tests (64, 128, 256 byte scenarios)
- ✅ Add cross-platform limit consistency tests
- ❌ **DO NOT modify cross-platform test logic**

### New Test Cases Required:

```cpp
TEST(D3D12Device, PushConstantLimitsAccurate) {
  auto device = createD3D12Device();
  auto limits = device->getFeatureLimits();

  // Verify reported limit matches implementation
  EXPECT_GE(limits.pushConstantsMaxSize, 64);   // Minimum viable
  EXPECT_LE(limits.pushConstantsMaxSize, 256);  // Spec maximum

  // Verify we can actually upload the reported amount
  auto commandBuffer = device->createCommandBuffer(...);
  auto encoder = commandBuffer->createRenderCommandEncoder(...);

  std::vector<uint32_t> data(limits.pushConstantsMaxSize / 4);
  encoder->bindBytes(0, data.data(), data.size() * 4);
  // Should NOT truncate or error
}

TEST(D3D12RenderCommandEncoder, BindBytesValidation) {
  auto encoder = createRenderEncoder();

  // Test 64-byte upload (should always work)
  uint32_t data64[16];
  encoder->bindBytes(0, data64, sizeof(data64));

  // Test 128-byte upload (should work after fix)
  uint32_t data128[32];
  encoder->bindBytes(0, data128, sizeof(data128));

  // Test 256-byte upload (should work with Option 3)
  uint32_t data256[64];
  encoder->bindBytes(0, data256, sizeof(data256));

  // Verify no truncation via shader readback
  // (Shader writes push constant data to UAV, CPU reads back and validates)
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass with accurate limits
./test_all_sessions.bat
```

**Expected Changes:**
- Sessions using push constants continue working
- No visual regressions
- Better cross-platform consistency

**Modifications Allowed:**
- ✅ None required (backend implementation change)
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] Reported limit matches actual encoder upload capacity
- [ ] D3D12 limit is consistent with Vulkan limit (±25%)
- [ ] Shader receives all pushed constant data (no truncation)
- [ ] Warning/error logged if app exceeds limit
- [ ] Documentation updated in code comments

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Cross-platform test suite passes (D3D12 and Vulkan)
2. Push constant upload test validates full advertised limit
3. No regressions in render sessions

**Post in chat:**
```
DX12-NEW-04 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Reported limit: XXX bytes (was 256 bytes)
- Actual capacity: XXX bytes (encoder verified)
- Cross-platform consistency: D3D12 XXX bytes, Vulkan XXX bytes

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix)

### Blocks:
- **C-003** (Root signature cost validation) - Must validate total DWORD budget if increasing to 256 bytes
- **DX12-NEW-06** (bindUniform stub) - Uniform binding may rely on push constants

### Related:
- **I-002** (Buffer alignment) - Similar cross-API limit inconsistency
- **I-001** (Vertex attribute limit) - Same pattern of over-reporting limits

---

## Implementation Priority

**Priority:** P1 - MEDIUM/HIGH (API Contract Violation)
**Estimated Effort:** 2-4 hours (Option 1), 4-6 hours (Option 2), 8-12 hours (Option 3)
**Risk:** Low (isolated change, well-tested)
**Impact:** HIGH - Fixes cross-platform portability and prevents silent data corruption

---

## References

- [Root Signatures - Root Constants](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures#root-constants)
- [Root Signature Limits](https://learn.microsoft.com/windows/win32/direct3d12/root-signature-limits)
- [VkPhysicalDeviceLimits::maxPushConstantsSize](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html)
- [Vulkan Push Constants Tutorial](https://vkguide.dev/docs/chapter-3/push_constants/)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
