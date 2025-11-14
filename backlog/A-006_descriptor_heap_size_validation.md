# A-006: No Validation of Descriptor Heap Size Limits

**Severity:** Medium
**Category:** Architecture Validation
**Status:** Open
**Related Issues:** A-003 (UMA Query), G-003 (Heap Fragmentation), C-002 (Double-Free Protection)

---

## Problem Statement

The `DescriptorHeapManager` initializes descriptor heaps with hardcoded sizes (256 CBV/SRV/UAV, 16 samplers, 64 RTVs, 32 DSVs) without validating these sizes against D3D12 device limits. Different GPU architectures have different maximum descriptor heap sizes, and exceeding these limits causes `CreateDescriptorHeap` to fail.

**Impact Analysis:**
- **Initialization Failures:** On lower-end GPUs or mobile devices, descriptor heap creation may fail silently with `FAILED(hr)` and no specific error message
- **Resource Exhaustion:** No warning when approaching heap capacity limits on resource-constrained devices
- **Portability:** Code that works on desktop GPUs (large heap support) may fail on integrated/mobile GPUs

**The Danger:**
- Application crashes on low-end hardware due to heap creation failure
- Silent failures where heaps are null but code continues, leading to access violations
- No diagnostic information about why initialization failed on specific hardware

---

## Root Cause Analysis

### Current Implementation (`DescriptorHeapManager.cpp:12-86`):

```cpp
Result DescriptorHeapManager::initialize(ID3D12Device* device, const Sizes& sizes) {
  if (!device) {
    return Result(Result::Code::ArgumentInvalid, "Null device for DescriptorHeapManager");
  }
  sizes_ = sizes; // NO VALIDATION against device limits!

  // Create shader-visible CBV/SRV/UAV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = sizes_.cbvSrvUav; // Uses size WITHOUT checking limits
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(cbvSrvUavHeap_.GetAddressOf())))) {
      return Result(Result::Code::RuntimeError, "Failed to create CBV/SRV/UAV heap");
      // ^^^ Generic error - no info about size limit violation!
    }
    // ... more heaps created similarly ...
  }
}
```

**Default sizes from `DescriptorHeapManager.h:21-24`:**
```cpp
struct Sizes {
  uint32_t cbvSrvUav = 256; // NO CHECK: May exceed device limit
  uint32_t samplers = 16;   // NO CHECK: May exceed D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE (2048)
  uint32_t rtvs = 64;       // NO CHECK: May exceed device limit
  uint32_t dsvs = 32;       // NO CHECK: May exceed device limit
};
```

### Why This Is Wrong:

1. **No Pre-Validation:** Sizes are used directly without querying device limits via `CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS)`
2. **Misleading Error Messages:** Generic "Failed to create heap" doesn't indicate if size was the problem
3. **Silent Clamping Opportunity Missed:** Could clamp to device limits with warning rather than failing outright
4. **No Logging of Limits:** Developers have no visibility into what the actual limits are on target hardware

---

## Official Documentation References

1. **Descriptor Heap Limits**:
   - https://learn.microsoft.com/windows/win32/direct3d12/hardware-support
   - Shader-visible CBV/SRV/UAV heap: Max 1,000,000 descriptors (Feature Level 11.0+)
   - Shader-visible sampler heap: Max 2,048 descriptors (`D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE`)
   - CPU-visible RTV/DSV heaps: Device-dependent, typically 64K+

2. **D3D12_FEATURE_D3D12_OPTIONS**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options
   - Contains `ResourceBindingTier` which affects descriptor table sizes
   - Tier 1: More restrictive limits on descriptor tables

3. **CreateDescriptorHeap Requirements**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap
   - Returns `E_INVALIDARG` if `NumDescriptors` exceeds device limits
   - Returns `E_OUTOFMEMORY` if allocation fails

4. **Best Practices**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingSimpleLighting/D3D12RaytracingSimpleLighting.cpp
   - Sample code validates heap sizes before creation

---

## Code Location Strategy

### Files to Modify:

1. **DescriptorHeapManager.cpp** (`initialize` method):
   - Search for: `sizes_ = sizes;`
   - Context: Beginning of initialization, before heap creation
   - Action: Insert validation logic to check sizes against device limits

2. **DescriptorHeapManager.cpp** (new private method):
   - Search for: End of class implementation, before namespace close
   - Context: Add new helper method
   - Action: Implement `validateAndClampSizes()` method

3. **Device.cpp** (`validateDeviceLimits` method):
   - Search for: Logging of `kMaxSamplers` validation
   - Context: Device limits validation section
   - Action: Add descriptor heap limit logging

---

## Detection Strategy

### How to Reproduce:

Modify descriptor heap sizes to exceed limits:
```cpp
// In test code
DescriptorHeapManager::Sizes sizes;
sizes.samplers = 3000; // Exceeds D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE (2048)

DescriptorHeapManager mgr;
Result result = mgr.initialize(device, sizes);
// Expected: Specific error about sampler limit
// Actual: Generic "Failed to create sampler heap"
```

### Verification After Fix:

1. Test with over-sized heaps shows specific validation error:
   ```
   WARNING: Requested sampler heap size (3000) exceeds device limit (2048)
   Clamping sampler heap to 2048 descriptors
   ```
2. Test with normal sizes passes without warnings
3. Logs during initialization show device-specific heap limits

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Validation Method Declaration

**File:** `src/igl/d3d12/DescriptorHeapManager.h`

**Locate:** Search for `private:` section with heap member variables

**Current (PROBLEM):**
```cpp
class DescriptorHeapManager {
 private:
  // Heaps
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;
  // ... more members ...

  // No validation method!
};
```

**Fixed (SOLUTION):**
```cpp
class DescriptorHeapManager {
 private:
  // Heaps
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;
  // ... more members ...

  // A-006: Validate and clamp descriptor heap sizes to device limits
  void validateAndClampSizes(ID3D12Device* device);
};
```

**Rationale:** Encapsulate validation logic in separate method for cleaner code organization.

---

#### Step 2: Implement Validation Logic

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** End of file, before namespace close `} // namespace igl::d3d12`

**Add New Method:**
```cpp
void DescriptorHeapManager::validateAndClampSizes(ID3D12Device* device) {
  // A-006: Validate descriptor heap sizes against D3D12 device limits
  IGL_LOG_INFO("=== Descriptor Heap Size Validation ===\n");

  // Query device options for resource binding tier (affects limits)
  D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
  HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                                            &options,
                                            sizeof(options));

  if (SUCCEEDED(hr)) {
    const char* tierName = "Unknown";
    switch (options.ResourceBindingTier) {
      case D3D12_RESOURCE_BINDING_TIER_1: tierName = "Tier 1"; break;
      case D3D12_RESOURCE_BINDING_TIER_2: tierName = "Tier 2"; break;
      case D3D12_RESOURCE_BINDING_TIER_3: tierName = "Tier 3"; break;
    }
    IGL_LOG_INFO("  Resource Binding Tier: %s\n", tierName);
  }

  // === SHADER-VISIBLE CBV/SRV/UAV HEAP ===
  // D3D12 spec: Max 1,000,000 descriptors for shader-visible heaps (FL 11.0+)
  // Conservative limit: 1,000,000 (actual limit may be lower on some hardware)
  constexpr uint32_t kMaxCbvSrvUavDescriptors = 1000000;

  if (sizes_.cbvSrvUav > kMaxCbvSrvUavDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested CBV/SRV/UAV heap size (%u) exceeds "
                  "D3D12 spec limit (%u)\n",
                  sizes_.cbvSrvUav, kMaxCbvSrvUavDescriptors);
    IGL_LOG_ERROR("  Clamping to %u descriptors\n", kMaxCbvSrvUavDescriptors);
    sizes_.cbvSrvUav = kMaxCbvSrvUavDescriptors;
  } else {
    IGL_LOG_INFO("  CBV/SRV/UAV heap size: %u (limit: %u) - OK\n",
                 sizes_.cbvSrvUav, kMaxCbvSrvUavDescriptors);
  }

  // === SHADER-VISIBLE SAMPLER HEAP ===
  // D3D12 spec: Max 2,048 descriptors (D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE)
  constexpr uint32_t kMaxSamplerDescriptors = 2048;

  if (sizes_.samplers > kMaxSamplerDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested sampler heap size (%u) exceeds "
                  "D3D12 limit (%u)\n",
                  sizes_.samplers, kMaxSamplerDescriptors);
    IGL_LOG_ERROR("  Clamping to %u descriptors\n", kMaxSamplerDescriptors);
    sizes_.samplers = kMaxSamplerDescriptors;
  } else {
    IGL_LOG_INFO("  Sampler heap size: %u (limit: %u) - OK\n",
                 sizes_.samplers, kMaxSamplerDescriptors);
  }

  // === CPU-VISIBLE RTV HEAP ===
  // D3D12 spec: Typically 64K+ descriptors (device-dependent)
  // Conservative validation: Warn if exceeding 16K (reasonable limit)
  constexpr uint32_t kMaxRtvDescriptors = 16384;

  if (sizes_.rtvs > kMaxRtvDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested RTV heap size (%u) is unusually large\n",
                  sizes_.rtvs);
    IGL_LOG_ERROR("  Recommended maximum: %u descriptors\n", kMaxRtvDescriptors);
    // Don't clamp - let CreateDescriptorHeap fail if truly excessive
  } else {
    IGL_LOG_INFO("  RTV heap size: %u (recommended max: %u) - OK\n",
                 sizes_.rtvs, kMaxRtvDescriptors);
  }

  // === CPU-VISIBLE DSV HEAP ===
  // Similar limits to RTV heap
  constexpr uint32_t kMaxDsvDescriptors = 16384;

  if (sizes_.dsvs > kMaxDsvDescriptors) {
    IGL_LOG_ERROR("  WARNING: Requested DSV heap size (%u) is unusually large\n",
                  sizes_.dsvs);
    IGL_LOG_ERROR("  Recommended maximum: %u descriptors\n", kMaxDsvDescriptors);
    // Don't clamp - let CreateDescriptorHeap fail if truly excessive
  } else {
    IGL_LOG_INFO("  DSV heap size: %u (recommended max: %u) - OK\n",
                 sizes_.dsvs, kMaxDsvDescriptors);
  }

  IGL_LOG_INFO("========================================\n");
}
```

**Rationale:**
- Validates shader-visible heaps against hard D3D12 limits (1M for CBV/SRV/UAV, 2048 for samplers)
- Warns about unusually large CPU-visible heaps but doesn't clamp (allows flexibility)
- Logs resource binding tier for diagnostic context
- Provides clear error messages indicating limit violations

---

#### Step 3: Call Validation in initialize()

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** Beginning of `initialize` method, after null check

**Current (PROBLEM):**
```cpp
Result DescriptorHeapManager::initialize(ID3D12Device* device, const Sizes& sizes) {
  if (!device) {
    return Result(Result::Code::ArgumentInvalid, "Null device for DescriptorHeapManager");
  }
  sizes_ = sizes; // Directly assigns without validation!

  // Create shader-visible CBV/SRV/UAV heap
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    // ... rest of heap creation ...
  }
}
```

**Fixed (SOLUTION):**
```cpp
Result DescriptorHeapManager::initialize(ID3D12Device* device, const Sizes& sizes) {
  if (!device) {
    return Result(Result::Code::ArgumentInvalid, "Null device for DescriptorHeapManager");
  }

  // A-006: Copy requested sizes, then validate/clamp against device limits
  sizes_ = sizes;
  validateAndClampSizes(device);

  // Create shader-visible CBV/SRV/UAV heap
  // Note: sizes_ may have been clamped by validation
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = sizes_.cbvSrvUav; // Now validated size
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(cbvSrvUavHeap_.GetAddressOf())))) {
      // A-006: Enhanced error message with size context
      IGL_LOG_ERROR("DescriptorHeapManager: Failed to create CBV/SRV/UAV heap "
                    "(size=%u descriptors)\n", sizes_.cbvSrvUav);
      return Result(Result::Code::RuntimeError, "Failed to create CBV/SRV/UAV heap");
    }
    // ... rest of heap creation ...
  }
}
```

**Rationale:** Validate before heap creation to catch limit violations early with specific error messages. Enhanced error logging includes size context for debugging.

---

#### Step 4: Add Improved Error Logging for All Heaps

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** Each `CreateDescriptorHeap` call in `initialize` method

**Current (PROBLEM):**
```cpp
// Sampler heap
if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(samplerHeap_.GetAddressOf())))) {
  return Result(Result::Code::RuntimeError, "Failed to create sampler heap");
  // Generic error - no size info
}

// RTV heap
if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap_.GetAddressOf())))) {
  return Result(Result::Code::RuntimeError, "Failed to create RTV heap");
  // Generic error - no size info
}

// DSV heap
if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(dsvHeap_.GetAddressOf())))) {
  return Result(Result::Code::RuntimeError, "Failed to create DSV heap");
  // Generic error - no size info
}
```

**Fixed (SOLUTION):**
```cpp
// Sampler heap - A-006: Enhanced error message
if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(samplerHeap_.GetAddressOf())))) {
  IGL_LOG_ERROR("DescriptorHeapManager: Failed to create sampler heap "
                "(size=%u descriptors, limit=2048)\n", sizes_.samplers);
  return Result(Result::Code::RuntimeError, "Failed to create sampler heap");
}

// RTV heap - A-006: Enhanced error message
if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap_.GetAddressOf())))) {
  IGL_LOG_ERROR("DescriptorHeapManager: Failed to create RTV heap "
                "(size=%u descriptors)\n", sizes_.rtvs);
  return Result(Result::Code::RuntimeError, "Failed to create RTV heap");
}

// DSV heap - A-006: Enhanced error message
if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(dsvHeap_.GetAddressOf())))) {
  IGL_LOG_ERROR("DescriptorHeapManager: Failed to create DSV heap "
                "(size=%u descriptors)\n", sizes_.dsvs);
  return Result(Result::Code::RuntimeError, "Failed to create DSV heap");
}
```

**Rationale:** Enhanced error messages help diagnose initialization failures by showing requested sizes and known limits.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Descriptor heap specific tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Descriptor*"
```

**Test Modifications Allowed:**
- ✅ Add test for heap size validation with over-sized requests
- ✅ Add test for heap size clamping behavior
- ❌ **DO NOT modify cross-platform test logic**

**New Tests to Add:**
```cpp
// Test that over-sized sampler heap is clamped
TEST(DescriptorHeapManager, ClampsSamplerHeapToLimit) {
  DescriptorHeapManager::Sizes sizes;
  sizes.samplers = 3000; // Exceeds 2048 limit

  DescriptorHeapManager mgr;
  Result result = mgr.initialize(testDevice, sizes);

  // Should succeed after clamping
  EXPECT_TRUE(result.isOk());
  // Verify heap was created (not null)
  EXPECT_NE(mgr.getSamplerHeap(), nullptr);
}

// Test that normal sizes pass validation
TEST(DescriptorHeapManager, AcceptsNormalSizes) {
  DescriptorHeapManager::Sizes sizes; // Use defaults (256, 16, 64, 32)

  DescriptorHeapManager mgr;
  Result result = mgr.initialize(testDevice, sizes);

  EXPECT_TRUE(result.isOk());
  EXPECT_NE(mgr.getCbvSrvUavHeap(), nullptr);
  EXPECT_NE(mgr.getSamplerHeap(), nullptr);
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should pass (validation doesn't change behavior for valid sizes)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required - validation is transparent for valid sizes
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (existing + new validation tests)
- [ ] `validateAndClampSizes()` method implemented
- [ ] Validation called in `initialize()` before heap creation
- [ ] Enhanced error messages include size context for all heap types
- [ ] Logs show validation results during initialization
- [ ] Over-sized sampler heap request is clamped to 2048
- [ ] Over-sized CBV/SRV/UAV heap request is clamped to 1,000,000
- [ ] Warnings logged for unusually large RTV/DSV heaps
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Initialization logs show descriptor heap size validation
2. Test with over-sized sampler heap (3000) shows clamping to 2048
3. All render sessions pass without warnings

**Post in chat:**
```
A-006 Fix Complete - Ready for Review

Descriptor Heap Size Validation:
- Validation: Checks against D3D12 limits before heap creation
- Clamping: Sampler heap clamped to 2048, CBV/SRV/UAV to 1M
- Logging: Shows validation results and resource binding tier
- Error Messages: Enhanced with size context

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Over-sized heap test: PASS (clamped correctly)
- Render sessions: PASS (no behavior change for valid sizes)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- **G-003** - Descriptor heap fragmentation (needs size validation as foundation)

### Related:
- **A-003** - UMA architecture query (both are device capability queries)
- **C-002** - Descriptor double-free protection (related to heap management)
- **P1_DX12-008** - Missing limit queries (similar validation approach)

---

## Implementation Priority

**Priority:** P1 - Medium (Architecture Validation)
**Estimated Effort:** 2-3 hours
**Risk:** Low (Validation-only change, improves error handling)
**Impact:** Prevents initialization failures on low-end hardware; improves diagnostic logging

**Notes:**
- Clamping strategy is conservative: Hard clamp shader-visible heaps (known limits), soft warn for CPU-visible heaps (device-dependent limits)
- Validation is performed during initialization, not during allocation (no performance impact)
- Enhanced error messages significantly improve debugging on diverse hardware

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/hardware-support
- https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap
- https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps
- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingSimpleLighting/D3D12RaytracingSimpleLighting.cpp

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
