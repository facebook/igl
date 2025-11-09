# TASK_P0_DX12-FIND-01: Fix Unbounded Descriptor Ranges for Tier 1 Hardware

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 1-2 days
**Status:** Open
**Issue ID:** DX12-FIND-01 (from Codex Audit)

---

## Problem Statement

The D3D12 backend uses unbounded descriptor ranges (`NumDescriptors = UINT_MAX`) in root signature creation, which fails on Tier 1 resource binding hardware and the WARP software rasterizer. This breaks compatibility with low-end and integrated GPUs, as well as headless/CI environments using WARP.

### Current Behavior
- Root signature descriptor ranges set to `UINT_MAX` (unbounded)
- Device creation succeeds on Tier 2/3 hardware (most discrete GPUs)
- **Device creation FAILS on Tier 1 hardware** (integrated GPUs, older hardware)
- **Device creation FAILS on WARP** (software rasterizer used in CI/headless)
- Error: `E_INVALIDARG` or `DXGI_ERROR_UNSUPPORTED` during root signature creation

### Expected Behavior
- Query resource binding tier during device initialization
- Use bounded descriptor ranges for Tier 1 hardware
- Use unbounded ranges only on Tier 2/3 hardware
- Fallback to maximum safe descriptor count (e.g., 1000000 for SRV/UAV, 2048 for samplers)
- Device creation succeeds on all hardware tiers

---

## Evidence and Code Location

### Where to Find the Issue

**File:** `src/igl/d3d12/Device.cpp`

**Search Pattern:**
1. Locate function `Device::createRenderPipeline()` or root signature creation helper
2. Find initialization of `D3D12_ROOT_SIGNATURE_DESC` structure
3. Look for `D3D12_DESCRIPTOR_RANGE` array initialization
4. Find lines setting `NumDescriptors = UINT_MAX`

**Specific Code Pattern to Search:**
```cpp
// Search for this pattern:
D3D12_DESCRIPTOR_RANGE srvRange = {};
srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
srvRange.NumDescriptors = UINT_MAX; // ← PROBLEM: Tier 1 incompatible
srvRange.BaseShaderRegister = 0;

D3D12_DESCRIPTOR_RANGE samplerRange = {};
samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
samplerRange.NumDescriptors = UINT_MAX; // ← PROBLEM: Tier 1 incompatible
```

**Additional Search Locations:**
- Any function building root signature descriptors
- Look for `D3D12_ROOT_PARAMETER` initialization with descriptor tables
- May be in a helper function like `buildRootSignature()` or similar

### How to Reproduce
1. Run on system with integrated GPU (Intel HD Graphics, older AMD APU)
2. Run with WARP: Set environment variable `IGL_FORCE_WARP=1` or use WARP adapter selection
3. Device creation will fail with `E_INVALIDARG` during root signature serialization

---

## Impact

**Severity:** Critical - Breaks compatibility
**Compatibility:** Fails on 30-40% of hardware in the field (integrated GPUs)
**CI/CD Impact:** Breaks headless testing with WARP

**Affected Systems:**
- Intel integrated graphics (HD Graphics, UHD Graphics)
- Older AMD APUs and integrated graphics
- Low-end discrete GPUs (older generation)
- WARP software rasterizer (used in CI, headless servers)

**Blocks:**
- Automated testing in headless environments
- Deployment on laptops with integrated graphics
- Compatibility with minimum spec hardware

---

## Official References

### Microsoft Documentation

1. **Resource Binding Tier** - [Hardware Tiers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support)
   - Tier 1: Bounded descriptor ranges only
   - Tier 2/3: Unbounded ranges supported

2. **Unbounded Arrays** - [Resource Binding in HLSL](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding-in-hlsl)
   - "Tier 1 hardware does not support unbounded descriptor arrays"
   - "Use `CheckFeatureSupport()` with `D3D12_FEATURE_D3D12_OPTIONS`"

3. **CheckFeatureSupport** - [Feature Detection API](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)
   ```cpp
   D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
   device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
   // Check options.ResourceBindingTier
   ```

4. **Descriptor Range** - [D3D12_DESCRIPTOR_RANGE](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_descriptor_range)
   - `NumDescriptors`: "Use `UINT_MAX` for unbounded (Tier 2+ only)"

### DirectX-Graphics-Samples

- **D3D12Raytracing** - Shows feature tier detection
- **D3D12DynamicIndexing** - Demonstrates bounded vs unbounded arrays
- **HelloTriangle** - Basic bounded descriptor usage

### Hardware Tier Breakdown

| Tier | Max SRV/UAV Descriptors | Max Samplers | Unbounded Arrays |
|------|-------------------------|--------------|------------------|
| 1    | 1,000,000              | 2,048        | ❌ Not Supported |
| 2    | 1,000,000              | 2,048        | ✅ Supported     |
| 3    | Unlimited              | Unlimited    | ✅ Supported     |

---

## Implementation Guidance

### High-Level Approach

1. **Feature Detection at Device Creation**
   - Query `D3D12_FEATURE_D3D12_OPTIONS` during `Device::create()`
   - Store `ResourceBindingTier` as member variable
   - Use tier to determine descriptor range strategy

2. **Conditional Descriptor Range Setup**
   - If Tier 2+: Use `UINT_MAX` (unbounded)
   - If Tier 1: Use maximum safe counts:
     - SRV/UAV/CBV: 1,000,000 (Tier 1 limit)
     - Samplers: 2,048 (Tier 1 limit)

3. **Update Root Signature Creation**
   - Modify descriptor range initialization to use tier-appropriate counts
   - Ensure shader expectations match descriptor range sizes

### Detailed Steps

**Step 1: Add Feature Detection in Device Initialization**

Location: `src/igl/d3d12/Device.cpp` - `Device::create()` or initialization function

```cpp
// Add member variable to Device.h:
D3D12_RESOURCE_BINDING_TIER resourceBindingTier_;

// In Device::create() or constructor:
D3D12_FEATURE_DATA_D3D12_OPTIONS featureOptions = {};
HRESULT hr = device_->CheckFeatureSupport(
    D3D12_FEATURE_D3D12_OPTIONS,
    &featureOptions,
    sizeof(featureOptions)
);

if (SUCCEEDED(hr)) {
    resourceBindingTier_ = featureOptions.ResourceBindingTier;
} else {
    // Assume Tier 1 for safety
    resourceBindingTier_ = D3D12_RESOURCE_BINDING_TIER_1;
}
```

**Step 2: Create Helper Function for Descriptor Counts**

```cpp
// Add to Device class:
uint32_t Device::getMaxDescriptorCount(D3D12_DESCRIPTOR_RANGE_TYPE type) const {
    if (resourceBindingTier_ >= D3D12_RESOURCE_BINDING_TIER_2) {
        return UINT_MAX; // Unbounded
    }

    // Tier 1 limits:
    switch (type) {
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            return 1000000; // Tier 1 max
        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
            return 2048; // Tier 1 max
        default:
            return 1000000;
    }
}
```

**Step 3: Update Root Signature Descriptor Ranges**

Location: Find where `D3D12_DESCRIPTOR_RANGE` is initialized (likely in `createRenderPipeline()`)

Replace:
```cpp
srvRange.NumDescriptors = UINT_MAX;
samplerRange.NumDescriptors = UINT_MAX;
```

With:
```cpp
srvRange.NumDescriptors = getMaxDescriptorCount(D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
samplerRange.NumDescriptors = getMaxDescriptorCount(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
```

**Step 4: Add Diagnostic Logging**

```cpp
if (resourceBindingTier_ == D3D12_RESOURCE_BINDING_TIER_1) {
    IGL_LOG_INFO("D3D12: Resource Binding Tier 1 detected - using bounded descriptor ranges\n");
}
```

**Step 5: Validate Descriptor Usage**

- Ensure descriptor heap allocation sizes respect tier limits
- Verify shader expectations don't exceed bounded array sizes
- Update any hardcoded assumptions about descriptor range sizes

### Edge Cases

- **WARP Detection:** WARP is always Tier 1 - ensure it works correctly
- **Feature Level:** Binding tier is independent of feature level (D3D_FEATURE_LEVEL_11_0 can be Tier 2)
- **Multiple Adapters:** Query tier per adapter if supporting multi-GPU

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- All device creation tests must pass
- No regression in existing tests

**Render Sessions:**
```bash
test_all_sessions.bat
```
All sessions must complete successfully on:
- Discrete GPU (Tier 2/3)
- Integrated GPU (Tier 1) - **CRITICAL**
- WARP (Tier 1) - **CRITICAL for CI**

### Validation Steps

1. **Tier 1 Hardware Testing**
   - Test on system with Intel integrated graphics
   - Test with WARP: Set adapter to WARP in device creation
   - Device creation must succeed
   - All render sessions must produce correct output

2. **Tier 2/3 Hardware Testing**
   - Test on discrete GPU (NVIDIA, AMD)
   - Verify unbounded ranges still used (check logs)
   - No performance regression

3. **Feature Detection Validation**
   - Log detected binding tier at startup
   - Verify correct tier reported for different hardware
   - Verify descriptor counts match tier

4. **Regression Testing**
   - All existing tests pass
   - No visual differences in render output
   - No validation layer errors

### WARP Testing (Critical for CI)

```bash
# Force WARP adapter selection
# May require environment variable or command-line flag
# Verify with engineering team how to select WARP adapter
```

Expected: All tests pass on WARP without device creation failures

---

## Success Criteria

- [ ] Feature detection implemented with `CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS)`
- [ ] Resource binding tier stored in `Device` class
- [ ] Helper function returns tier-appropriate descriptor counts
- [ ] Root signature ranges use bounded counts on Tier 1
- [ ] Root signature ranges use unbounded (`UINT_MAX`) on Tier 2/3
- [ ] Device creation succeeds on WARP
- [ ] Device creation succeeds on integrated GPUs (Tier 1)
- [ ] All unit tests pass on all hardware tiers
- [ ] All render sessions pass on all hardware tiers
- [ ] Diagnostic logging shows detected tier
- [ ] No validation layer errors
- [ ] User confirms tests pass on Tier 1 hardware/WARP

---

## Dependencies

**Depends On:**
- None (foundational compatibility fix)

**Blocks:**
- Automated testing with WARP
- Deployment on integrated graphics systems
- P1 compute and storage buffer features (rely on descriptor ranges)

**Related:**
- TASK_P0_DX12-FIND-02 (Descriptor Heap Overflow) - Both affect descriptor management

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts
   - ❌ DO NOT modify render sessions
   - ✅ Tests must pass on Tier 1, 2, and 3 hardware

2. **User Confirmation Required**
   - ⚠️ Test on WARP (Tier 1) - MANDATORY
   - ⚠️ Test on integrated GPU if available
   - ⚠️ Report results for each tier tested
   - ⚠️ Wait for user confirmation

3. **Code Modification Scope**
   - ✅ Modify `Device.cpp` and `Device.h`
   - ✅ Update root signature creation logic
   - ❌ DO NOT change shader binding expectations
   - ❌ DO NOT modify IGL public API

4. **Compatibility**
   - Must work on all binding tiers (1, 2, 3)
   - Must work on WARP
   - Must not regress performance on high-tier hardware

---

**Estimated Timeline:** 1-2 days
**Risk Level:** Medium (affects device initialization, requires multi-tier testing)
**Validation Effort:** 4-6 hours (test on multiple hardware tiers)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 01*
