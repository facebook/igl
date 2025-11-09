# TASK_P2_DX12-FIND-09: Increase Push Constant Budget to Match Vulkan

**Priority:** P2 - Medium
**Estimated Effort:** 4-6 hours
**Status:** Open
**Issue ID:** DX12-FIND-09 (from Codex Audit)

---

## Problem Statement

Push constants are limited to 16 DWORDs (64 bytes) on D3D12 backend, while Vulkan exposes 128 bytes by default and D3D12 spec allows up to 64 DWORDs (256 bytes). Shaders using >64 bytes of push constants fail on D3D12 but work on Vulkan.

### Current Behavior
- Push constants: 16 DWORDs (64 bytes) for graphics and compute
- Vulkan backend: 128 bytes
- D3D12 spec allows: 64 DWORDs (256 bytes)
- Breaks cross-platform shader compatibility

### Expected Behavior
- Expose at least 128 bytes (32 DWORDs) to match Vulkan
- Optionally support full 256 bytes (64 DWORDs)
- Portable shaders work across all backends

---

## Evidence and Code Location

**File:** `src/igl/d3d12/Device.cpp`

**Search Locations:**
- Line 736-747: Compute pipeline push constant setup
- Line 932-943: Graphics pipeline push constant setup

**Search Keywords:**
- Push constant
- Root constant
- 16 DWORD
- `D3D12_ROOT_CONSTANTS`

**Comparison:**
- **Vulkan:** `src/igl/vulkan/PipelineState.cpp:55-70` - 128 bytes

---

## Impact

**Severity:** Medium - Breaks shader portability
**Portability:** Shaders using >64 bytes fail on D3D12
**Cross-API:** Violates parity with Vulkan

**Affected Shaders:**
- Complex shaders with many uniforms
- Shaders using 96-128 bytes of push constants
- Works on Vulkan, fails on D3D12

---

## Official References

### Microsoft Documentation
1. **Root Constants** - [D3D12_ROOT_CONSTANTS](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_constants)
   - Maximum: 64 DWORDs (256 bytes)

2. **Root Signature Cost** - Root constants cheap, prefer over CBVs for small data

### Vulkan Reference
- **VkPhysicalDeviceLimits** - `maxPushConstantsSize` typically 128-256 bytes

---

## Implementation Guidance

### High-Level Approach

1. **Increase Push Constant Budget**
   - Change from 16 DWORDs → 32 DWORDs (128 bytes) for Vulkan parity
   - Or 64 DWORDs (256 bytes) for D3D12 maximum

2. **Update Root Signature**
   ```cpp
   D3D12_ROOT_CONSTANTS rootConstants = {};
   rootConstants.ShaderRegister = 0;  // b0
   rootConstants.RegisterSpace = 0;
   rootConstants.Num32BitValues = 32;  // 128 bytes (or 64 for 256 bytes)
   ```

3. **Validate Per-Shader**
   - Check shader doesn't exceed budget
   - Return error if shader requires too many push constants

### Detailed Steps

**Step 1: Find Push Constant Configuration**

In `Device::createRenderPipeline()` and `createComputePipeline()`:
```cpp
// Current:
rootConstants.Num32BitValues = 16;  // 64 bytes

// Change to:
rootConstants.Num32BitValues = 32;  // 128 bytes (Vulkan parity)
// Or:
rootConstants.Num32BitValues = 64;  // 256 bytes (D3D12 max)
```

**Step 2: Update IGL Constant**

If there's a shared constant:
```cpp
constexpr uint32_t kMaxPushConstantSize = 128;  // bytes
constexpr uint32_t kMaxPushConstantDWORDs = kMaxPushConstantSize / 4;
```

**Step 3: Add Validation**

```cpp
if (shader->pushConstantSize > kMaxPushConstantSize) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "Push constants exceed maximum size");
}
```

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Validation Steps

1. **Large Push Constant Test**
   - Create shader with 128 bytes of push constants
   - Verify it works on D3D12
   - Should match Vulkan behavior

2. **Cross-API Test**
   - Same shader should work on D3D12 and Vulkan
   - Verify identical results

---

## Success Criteria

- [ ] Push constant budget increased to ≥128 bytes
- [ ] Root signature updated for graphics pipeline
- [ ] Root signature updated for compute pipeline
- [ ] Shaders with 128 bytes work on D3D12
- [ ] All tests pass
- [ ] Cross-API parity with Vulkan
- [ ] User confirms increased budget works

---

## Dependencies

**Related:**
- Cross-API portability requirements

---

**Estimated Timeline:** 4-6 hours
**Risk Level:** Low (configuration change)
**Validation Effort:** 2 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 09*
