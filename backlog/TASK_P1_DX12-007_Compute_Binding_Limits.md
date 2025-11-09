# TASK_P1_DX12-007: Align Compute Shader Binding Limits to IGL Contract

**Priority:** P1 - High
**Estimated Effort:** 2-3 hours
**Status:** Open

---

## Problem Statement

The D3D12 backend exposes only 8 compute shader binding slots (textures/buffers), while the IGL contract guarantees 16 slots (`IGL_TEXTURE_SAMPLERS_MAX=16`). This breaks cross-API portability as shaders using 9-16 binding slots work on Vulkan/OpenGL but fail on D3D12.

### Current Behavior
- Compute pipeline configured with 8 binding slots
- IGL contract promises 16 slots minimum
- Shaders using slots 8-15 fail on D3D12 (work on Vulkan/OpenGL)
- Cross-API portability broken

### Expected Behavior
- D3D12 backend exposes 16 compute binding slots
- Matches Vulkan/OpenGL backends
- Portable compute shaders work across all backends

---

## Evidence and Code Location

**Search Pattern:**
1. Find compute pipeline creation in `Device.cpp`
2. Look for root signature creation for compute
3. Find binding slot limit configuration
4. Search for `IGL_TEXTURE_SAMPLERS_MAX` usage

**Files to Check:**
- `src/igl/d3d12/Device.cpp` - Compute pipeline creation
- `src/igl/d3d12/ComputeCommandEncoder.cpp` - Binding logic
- `src/igl/Common.h` - IGL constant definitions

**Search Keywords:**
- Compute root signature
- Compute descriptor table
- Binding slot limit
- `IGL_TEXTURE_SAMPLERS_MAX`

---

## Impact

**Severity:** High - Breaks cross-API portability
**Portability:** Shaders fail on D3D12 but work on other backends
**API Contract:** Violates IGL minimum guarantees

**Affected Code Paths:**
- Compute shaders using >8 bindings
- Portable shader code
- Complex compute pipelines

---

## Official References

### IGL Contract
```cpp
// src/igl/Common.h
#define IGL_TEXTURE_SAMPLERS_MAX 16
#define IGL_UNIFORM_BLOCKS_BINDING_MAX 16
```

### Microsoft Documentation
- D3D12 supports many more than 16 bindings (Tier 2+: unbounded)
- No technical limitation preventing 16 slots

### Cross-API Comparison
- **Vulkan:** Exposes 16+ descriptor bindings
- **OpenGL:** `GL_MAX_*` constants >= 16
- **D3D12:** Currently artificially limited to 8

---

## Implementation Guidance

### High-Level Approach

1. **Find Compute Root Signature Creation**
   - Locate where compute root signature descriptor ranges are defined
   - Find where descriptor table size is configured

2. **Increase Descriptor Range Size**
   - Change from 8 to 16 descriptors
   - For SRV/UAV/CBV tables

3. **Update Binding Logic**
   - Ensure encoder can handle 16 bindings
   - Verify descriptor allocation supports this

### Detailed Steps

**Step 1: Locate Compute Root Signature Setup**

In `Device::createComputePipeline()` or similar:
```cpp
// Find pattern like:
D3D12_DESCRIPTOR_RANGE srvRange = {};
srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
srvRange.NumDescriptors = 8;  // ← CHANGE TO 16
```

**Step 2: Update Descriptor Ranges**

Change all compute-related descriptor ranges:
```cpp
// SRV range
srvRange.NumDescriptors = IGL_TEXTURE_SAMPLERS_MAX;  // 16

// UAV range (if separate)
uavRange.NumDescriptors = IGL_TEXTURE_SAMPLERS_MAX;  // 16

// CBV range (if needed)
cbvRange.NumDescriptors = IGL_UNIFORM_BLOCKS_BINDING_MAX;  // 16
```

**Step 3: Verify Descriptor Heap Capacity**

Ensure per-frame descriptor heap can accommodate:
- Graphics bindings + Compute bindings
- May need to increase heap size if close to 1024 limit

**Step 4: Update Encoder Binding Logic**

In `ComputeCommandEncoder::bind*()` functions:
- Verify no hardcoded limits at 8
- Ensure all 16 slots can be bound

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

**Critical Sessions:**
- ComputeSession (if exists)
- Any compute-based session

### Validation Steps

1. **Binding Limit Test**
   - Create compute shader using 16 binding slots
   - Verify all slots can be bound
   - Check shader executes correctly

2. **Cross-API Parity**
   - Same shader should work on D3D12, Vulkan, OpenGL
   - Verify identical behavior

3. **Regression**
   - Existing compute shaders still work
   - No performance degradation

---

## Success Criteria

- [ ] Compute root signature supports 16 SRV/UAV/CBV slots
- [ ] Descriptor ranges set to `IGL_TEXTURE_SAMPLERS_MAX` (16)
- [ ] All 16 slots can be bound successfully
- [ ] Test shader using 16 slots works
- [ ] All tests pass
- [ ] User confirms compute parity with Vulkan/OpenGL

---

## Dependencies

**Depends On:**
- TASK_P1_DX12-FIND-04 (Compute CBV Binding) - May be implemented together

**Related:**
- Cross-API portability requirements

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **IGL Contract:** Must match `IGL_TEXTURE_SAMPLERS_MAX=16`
3. **Cross-API:** Must match Vulkan/OpenGL capabilities

---

**Estimated Timeline:** 2-3 hours
**Risk Level:** Low-Medium (configuration change)
**Validation Effort:** 2 hours

---

*Task Created: 2025-11-08*
