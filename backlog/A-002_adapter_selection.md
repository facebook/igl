# A-002: Adapter Selection Doesn't Prefer High-Performance GPU

**Priority**: MEDIUM
**Category**: Architecture - Device Initialization
**Estimated Effort**: 2-3 days
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend does not intelligently select the best GPU when multiple adapters are available. On systems with both integrated and discrete GPUs (common in laptops), the backend may select the lower-performance integrated GPU instead of the high-performance discrete GPU, resulting in poor rendering performance.

**Symptoms**:
- Application runs on integrated GPU (Intel/AMD iGPU) instead of discrete GPU (NVIDIA/AMD dGPU)
- Lower frame rates than expected on systems with dedicated graphics
- Users manually need to force GPU selection via driver control panels
- Inconsistent performance across different hardware configurations

**Impact**:
- Significantly degraded performance on laptops with dual GPUs
- Poor user experience due to unexpected low performance
- Support burden from users confused about performance
- Application appears poorly optimized when running on wrong GPU

---

## 2. Root Cause Analysis

The adapter selection issue stems from several possible causes:

### 2.1 Default Adapter Selection
The code may simply use the first adapter returned by `EnumAdapters()` or `EnumAdapters1()`, which is not guaranteed to be the high-performance GPU.

**Problematic Pattern**:
```cpp
ComPtr<IDXGIAdapter1> adapter;
factory->EnumAdapters1(0, &adapter);  // Just use first adapter
```

### 2.2 No Performance Heuristics
The code doesn't query adapter properties (dedicated video memory, performance tier, power preference) to determine which is the high-performance GPU.

### 2.3 No DXGI_GPU_PREFERENCE Usage
Modern DXGI (1.6+) provides `EnumAdapterByGpuPreference()` to request high-performance adapters, but this may not be used.

### 2.4 Software Adapter Not Filtered
The WARP (Windows Advanced Rasterization Platform) software adapter may be selected instead of hardware adapters.

### 2.5 No User Override Option
Even if automatic selection is improved, there's no way for users or developers to override the adapter choice for testing or specific scenarios.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Adapter Enumeration**:
   - https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-enumadapters
   - Basic adapter enumeration methods

2. **GPU Preference Selection (DXGI 1.6)**:
   - https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference
   - Modern API for selecting high-performance adapters

3. **DXGI_GPU_PREFERENCE Enumeration**:
   - https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_6/ne-dxgi1_6-dxgi_gpu_preference
   - Enum values: UNSPECIFIED, MINIMUM_POWER, HIGH_PERFORMANCE

4. **Adapter Description**:
   - https://docs.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1
   - Properties for identifying adapter capabilities

5. **Hybrid Graphics Systems**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3darticles/directx-graphics-articles-hybrid-graphics
   - Overview of multi-GPU systems and selection strategies

**Sample Code**:

6. **D3D12 Device Creation Sample**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
   - Shows proper adapter enumeration

7. **DXGI Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld
   - Various adapter selection patterns

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for adapter enumeration**:
```
Pattern: "EnumAdapters" OR "IDXGIAdapter"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/HWDevice.cpp
Focus: Where adapters are enumerated and selected
```

**Search for device creation**:
```
Pattern: "D3D12CreateDevice" OR "CreateDevice"
Files: src/igl/d3d12/Device.cpp
Focus: Where D3D12 device is created with selected adapter
```

**Search for factory creation**:
```
Pattern: "CreateDXGIFactory" OR "IDXGIFactory"
Files: src/igl/d3d12/**/*.cpp
Focus: DXGI factory initialization
```

**Search for adapter description queries**:
```
Pattern: "GetDesc" OR "DXGI_ADAPTER_DESC"
Files: src/igl/d3d12/**/*.cpp
Focus: Where adapter properties are queried
```

### 4.2 Likely File Locations

Based on typical D3D12 backend architecture:
- `src/igl/d3d12/Device.cpp` - Main device and adapter selection logic
- `src/igl/d3d12/HWDevice.cpp` - Hardware device initialization
- `src/igl/d3d12/Common.h` - May contain adapter selection utilities
- Platform-specific initialization files

### 4.3 Key Code Patterns to Find

Look for:
- `CreateDXGIFactory1()` or `CreateDXGIFactory2()` calls
- `EnumAdapters()` or `EnumAdapters1()` calls
- `D3D12CreateDevice()` calls
- Adapter selection loops or logic
- Logging of selected adapter name

---

## 5. Detection Strategy

### 5.1 Add Instrumentation Code

**Step 1: Log all available adapters**

Add comprehensive adapter enumeration logging:

```cpp
void logAvailableAdapters(IDXGIFactory1* factory) {
    IGL_LOG_INFO("=== Available D3D12 Adapters ===\n");

    UINT adapterIndex = 0;
    ComPtr<IDXGIAdapter1> adapter;

    while (factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Convert wide string to narrow
        char adapterName[128];
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName, sizeof(adapterName), nullptr, nullptr);

        IGL_LOG_INFO("Adapter %u: %s\n", adapterIndex, adapterName);
        IGL_LOG_INFO("  Vendor ID: 0x%04X\n", desc.VendorId);
        IGL_LOG_INFO("  Device ID: 0x%04X\n", desc.DeviceId);
        IGL_LOG_INFO("  Dedicated Video Memory: %llu MB\n", desc.DedicatedVideoMemory / (1024 * 1024));
        IGL_LOG_INFO("  Dedicated System Memory: %llu MB\n", desc.DedicatedSystemMemory / (1024 * 1024));
        IGL_LOG_INFO("  Shared System Memory: %llu MB\n", desc.SharedSystemMemory / (1024 * 1024));
        IGL_LOG_INFO("  Flags: 0x%X %s\n", desc.Flags,
            (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? "(SOFTWARE)" :
            (desc.Flags & DXGI_ADAPTER_FLAG_REMOTE) ? "(REMOTE)" : "(HARDWARE)");

        adapterIndex++;
        adapter.Reset();
    }

    IGL_LOG_INFO("=================================\n");
}
```

**Step 2: Log selected adapter**

```cpp
void logSelectedAdapter(IDXGIAdapter1* selectedAdapter) {
    DXGI_ADAPTER_DESC1 desc;
    selectedAdapter->GetDesc1(&desc);

    char adapterName[128];
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName, sizeof(adapterName), nullptr, nullptr);

    IGL_LOG_INFO("*** SELECTED ADAPTER: %s ***\n", adapterName);
    IGL_LOG_INFO("*** Dedicated Video Memory: %llu MB ***\n", desc.DedicatedVideoMemory / (1024 * 1024));
}
```

### 5.2 Manual Testing Procedure

**Test 1: Run on laptop with dual GPUs**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

Check console output for adapter list and selected adapter. On a system with both integrated and discrete GPU, verify the discrete GPU is selected.

**Test 2: Run sample applications**
```bash
./build/Debug/samples/Textured3DCube
./build/Debug/samples/TQSession
```

Check which adapter is being used.

**Test 3: Verify with external tools**

Use GPU-Z, Task Manager (Performance tab), or NVIDIA/AMD control panels to verify which GPU is being used during application execution.

### 5.3 Expected Baseline Metrics

**Current (Problematic) Behavior**:
- First adapter in list is selected (often integrated GPU)
- No preference for high-performance GPU
- Selection may vary between systems unpredictably

**Target Behavior**:
- High-performance discrete GPU selected when available
- Integrated GPU only used if it's the only hardware adapter
- Software adapter (WARP) only used as fallback
- Consistent selection logic across systems

---

## 6. Fix Guidance

### 6.1 Implement Modern Adapter Selection (DXGI 1.6)

**Step 1: Use EnumAdapterByGpuPreference (preferred approach)**

```cpp
// In Device initialization
Result Device::selectAdapter() {
    ComPtr<IDXGIFactory6> factory6;
    HRESULT hr = factory_.As(&factory6);

    if (SUCCEEDED(hr)) {
        // DXGI 1.6 available - use GPU preference API
        IGL_LOG_INFO("Using DXGI 1.6 adapter selection with HIGH_PERFORMANCE preference\n");

        hr = factory6->EnumAdapterByGpuPreference(
            0,  // First adapter matching preference
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter_)
        );

        if (SUCCEEDED(hr) && adapter_) {
            logSelectedAdapter(adapter_.Get());
            return Result();
        }

        IGL_LOG_WARNING("EnumAdapterByGpuPreference failed, falling back to manual selection\n");
    }

    // Fallback for older Windows versions
    return selectAdapterFallback();
}
```

**Step 2: Implement fallback adapter selection**

For systems without DXGI 1.6 (Windows 10 before version 1803):

```cpp
Result Device::selectAdapterFallback() {
    IGL_LOG_INFO("Using fallback adapter selection (DXGI < 1.6)\n");

    ComPtr<IDXGIAdapter1> bestAdapter;
    SIZE_T maxDedicatedMemory = 0;

    UINT adapterIndex = 0;
    ComPtr<IDXGIAdapter1> currentAdapter;

    while (factory_->EnumAdapters1(adapterIndex, &currentAdapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        currentAdapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            IGL_LOG_DEBUG("Skipping software adapter: %ws\n", desc.Description);
            adapterIndex++;
            currentAdapter.Reset();
            continue;
        }

        // Check if this adapter supports D3D12
        if (SUCCEEDED(D3D12CreateDevice(currentAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
            // Prefer adapter with most dedicated video memory
            if (desc.DedicatedVideoMemory > maxDedicatedMemory) {
                maxDedicatedMemory = desc.DedicatedVideoMemory;
                bestAdapter = currentAdapter;

                IGL_LOG_INFO("Found better adapter: %ws (%llu MB)\n",
                    desc.Description, desc.DedicatedVideoMemory / (1024 * 1024));
            }
        }

        adapterIndex++;
        currentAdapter.Reset();
    }

    if (bestAdapter) {
        adapter_ = bestAdapter;
        logSelectedAdapter(adapter_.Get());
        return Result();
    }

    // No hardware adapter found, try WARP as last resort
    return selectWARPAdapter();
}
```

**Step 3: Implement WARP fallback**

```cpp
Result Device::selectWARPAdapter() {
    IGL_LOG_WARNING("No hardware adapter found, using WARP software adapter\n");

    HRESULT hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter_));
    if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create WARP adapter");
    }

    logSelectedAdapter(adapter_.Get());
    return Result();
}
```

### 6.2 Add Vendor-Specific Heuristics

Improve selection with vendor identification:

```cpp
enum class GPUVendor {
    Unknown,
    NVIDIA,
    AMD,
    Intel,
    Microsoft  // WARP
};

GPUVendor getVendor(UINT vendorId) {
    switch (vendorId) {
        case 0x10DE: return GPUVendor::NVIDIA;
        case 0x1002: return GPUVendor::AMD;
        case 0x8086: return GPUVendor::Intel;
        case 0x1414: return GPUVendor::Microsoft;
        default: return GPUVendor::Unknown;
    }
}

// In adapter scoring
int scoreAdapter(const DXGI_ADAPTER_DESC1& desc) {
    int score = 0;

    // Heavily favor discrete GPUs (NVIDIA/AMD)
    GPUVendor vendor = getVendor(desc.VendorId);
    if (vendor == GPUVendor::NVIDIA || vendor == GPUVendor::AMD) {
        score += 1000;
    } else if (vendor == GPUVendor::Intel) {
        score += 100;  // Integrated GPU
    }

    // Add points for video memory (1 point per 64 MB)
    score += static_cast<int>(desc.DedicatedVideoMemory / (64 * 1024 * 1024));

    return score;
}

// Use in selection
if (scoreAdapter(currentDesc) > scoreAdapter(bestDesc)) {
    bestAdapter = currentAdapter;
}
```

### 6.3 Add Configuration Override

Allow explicit adapter selection:

```cpp
// In Device configuration or initialization
struct DeviceConfig {
    int preferredAdapterIndex = -1;  // -1 = auto, >= 0 = specific adapter
    bool allowWARP = false;  // Allow software adapter
    bool forceHighPerformance = true;  // Prefer high-performance GPU
};

Result Device::selectAdapter(const DeviceConfig& config) {
    // Log all available adapters first
    logAvailableAdapters(factory_.Get());

    // Check for explicit adapter index override
    if (config.preferredAdapterIndex >= 0) {
        ComPtr<IDXGIAdapter1> adapter;
        HRESULT hr = factory_->EnumAdapters1(config.preferredAdapterIndex, &adapter);
        if (SUCCEEDED(hr)) {
            adapter_ = adapter;
            IGL_LOG_INFO("Using explicitly selected adapter index %d\n", config.preferredAdapterIndex);
            logSelectedAdapter(adapter_.Get());
            return Result();
        } else {
            IGL_LOG_WARNING("Failed to select adapter index %d, using auto-selection\n",
                config.preferredAdapterIndex);
        }
    }

    // Auto-select based on performance preference
    if (config.forceHighPerformance) {
        return selectAdapter();  // Use high-performance selection
    } else {
        // Could add MINIMUM_POWER preference for battery-powered scenarios
        return selectAdapterMinimumPower();
    }
}
```

### 6.4 Environment Variable Override

Add debug override capability:

```cpp
Result Device::selectAdapter() {
    // Check for environment variable override
    const char* adapterIndexEnv = std::getenv("IGL_D3D12_ADAPTER");
    if (adapterIndexEnv) {
        int adapterIndex = std::atoi(adapterIndexEnv);
        IGL_LOG_INFO("IGL_D3D12_ADAPTER environment variable set to %d\n", adapterIndex);

        ComPtr<IDXGIAdapter1> adapter;
        if (SUCCEEDED(factory_->EnumAdapters1(adapterIndex, &adapter))) {
            adapter_ = adapter;
            logSelectedAdapter(adapter_.Get());
            return Result();
        } else {
            IGL_LOG_ERROR("Failed to select adapter %d from environment variable\n", adapterIndex);
        }
    }

    // Proceed with normal selection
    // ...
}
```

### 6.5 Update Device Creation

Ensure selected adapter is used:

```cpp
Result Device::createDevice() {
    if (!adapter_) {
        return Result(Result::Code::RuntimeError, "No adapter selected");
    }

    // Create device with selected adapter
    HRESULT hr = D3D12CreateDevice(
        adapter_.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_)
    );

    if (FAILED(hr)) {
        return getResultFromHRESULT(hr);
    }

    IGL_LOG_INFO("D3D12 device created successfully\n");
    return Result();
}
```

---

## 7. Testing Requirements

### 7.1 Functional Testing

**Test 1: Basic functionality on all systems**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: All tests pass on systems with single or multiple GPUs.

**Test 2: Verify adapter logging**

Check console output includes:
```
=== Available D3D12 Adapters ===
Adapter 0: NVIDIA GeForce RTX 3060
  Vendor ID: 0x10DE
  Dedicated Video Memory: 6144 MB
Adapter 1: Intel(R) UHD Graphics 630
  Vendor ID: 0x8086
  Dedicated Video Memory: 128 MB
=================================
*** SELECTED ADAPTER: NVIDIA GeForce RTX 3060 ***
```

**Test 3: Test on different hardware configurations**
- Desktop with single discrete GPU
- Laptop with integrated + discrete GPU
- System with integrated GPU only
- Virtual machine (may use software adapter)

### 7.2 Validation Testing

**Test 1: Verify correct adapter on dual-GPU laptop**

On a laptop with both integrated and discrete GPU:
1. Run application
2. Open Task Manager → Performance tab
3. Verify GPU 1 (discrete) shows activity, not GPU 0 (integrated)

**Test 2: Test environment variable override**

```bash
set IGL_D3D12_ADAPTER=0
./build/Debug/samples/Textured3DCube

set IGL_D3D12_ADAPTER=1
./build/Debug/samples/Textured3DCube
```

Verify correct adapter is used in each case.

**Test 3: Test WARP fallback**

On a system without hardware D3D12 support (or by forcing WARP), verify the application doesn't crash and uses WARP adapter.

### 7.3 Performance Testing

**Before/After Comparison**:

On dual-GPU laptop, measure frame rate:
- Before: ~30 FPS (integrated GPU)
- After: ~120 FPS (discrete GPU)

Performance should match or exceed native discrete GPU performance.

### 7.4 Test Modification Restrictions

**CRITICAL CONSTRAINTS**:
- Do NOT modify any existing test assertions
- Do NOT change test expected values
- Do NOT alter test logic or flow
- Changes must be implementation-only, not test-visible
- All tests must pass without modification

Adapter selection is transparent to tests.

---

## 8. Definition of Done

### 8.1 Implementation Checklist

- [ ] DXGI 1.6 EnumAdapterByGpuPreference implemented
- [ ] Fallback adapter selection for older Windows versions
- [ ] Adapter scoring based on dedicated video memory
- [ ] Software adapter (WARP) filtering
- [ ] Vendor-specific heuristics (prefer NVIDIA/AMD over Intel)
- [ ] Configuration override support
- [ ] Environment variable override (IGL_D3D12_ADAPTER)
- [ ] Comprehensive adapter logging
- [ ] WARP fallback as last resort

### 8.2 Testing Checklist

- [ ] All unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"`
- [ ] Discrete GPU selected on dual-GPU systems
- [ ] Integrated GPU selected when it's the only option
- [ ] Adapter logging shows all available adapters
- [ ] Environment variable override works correctly
- [ ] No crashes on any hardware configuration
- [ ] Performance matches discrete GPU capabilities

### 8.3 Documentation Checklist

- [ ] Adapter selection logic documented
- [ ] Configuration options documented
- [ ] Environment variable override documented
- [ ] Troubleshooting guide for adapter issues

### 8.4 Sign-Off Criteria

**Before proceeding with this task, YOU MUST**:
1. Read and understand all 11 sections of this document
2. Understand DXGI adapter enumeration APIs
3. Have access to dual-GPU system for testing (or understand limitations)
4. Confirm you can build and run the test suite
5. Get explicit approval to proceed with implementation

**Do not make any code changes until all criteria are met and approval is given.**

---

## 9. Related Issues

### 9.1 Blocking Issues
None - this can be implemented independently.

### 9.2 Blocked Issues
- Proper adapter selection is prerequisite for accurate performance testing
- Affects all performance-related issues

### 9.3 Related Issues
- May expose performance issues that were hidden by integrated GPU
- Required for multi-GPU configurations (future)

---

## 10. Implementation Priority

**Priority Level**: MEDIUM

**Rationale**:
- Critical for dual-GPU systems (common in laptops)
- Significant user-visible performance impact
- Relatively simple implementation
- Low risk (fallback to current behavior if needed)
- Standard D3D12 best practice

**Recommended Order**:
1. Add adapter logging (Day 1)
2. Implement DXGI 1.6 selection (Day 1)
3. Implement fallback selection (Day 2)
4. Add configuration/override options (Day 2)
5. Testing on various hardware (Day 3)

**Estimated Timeline**:
- Day 1: Investigation, instrumentation, DXGI 1.6 implementation
- Day 2: Fallback implementation, configuration options
- Day 3: Testing on different hardware configurations

---

## 11. References

### 11.1 Microsoft Official Documentation
- Adapter Enumeration: https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-enumadapters
- EnumAdapterByGpuPreference: https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference
- DXGI_GPU_PREFERENCE: https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_6/ne-dxgi1_6-dxgi_gpu_preference
- DXGI_ADAPTER_DESC1: https://docs.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1
- Hybrid Graphics: https://docs.microsoft.com/en-us/windows/win32/direct3darticles/directx-graphics-articles-hybrid-graphics

### 11.2 Sample Code References
- D3D12HelloTriangle: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
- DXGI Samples: https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld

### 11.3 Additional Reading
- GPU Vendor IDs: https://pcisig.com/membership/member-companies (NVIDIA: 0x10DE, AMD: 0x1002, Intel: 0x8086)
- Hybrid Graphics Best Practices: https://developer.nvidia.com/hybrid-graphics

### 11.4 Internal Codebase
- Search for "EnumAdapters" in src/igl/d3d12/
- Review current adapter selection in Device.cpp
- Check for existing configuration infrastructure

---

**Document Version**: 1.0
**Last Updated**: 2025-11-12
**Author**: Development Team
**Reviewer**: [Pending]
