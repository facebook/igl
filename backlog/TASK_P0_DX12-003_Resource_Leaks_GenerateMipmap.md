# TASK_P0_DX12-003: Fix Resource Leaks in Texture::generateMipmap()

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 4-6 hours
**Status:** Open

---

## Problem Statement

The `Texture::generateMipmap()` function in the D3D12 backend contains multiple resource leak paths. When errors occur during mipmap generation, the function returns early without releasing previously allocated D3D12 resources (descriptor heaps, pipeline state objects, root signatures, intermediate textures). Over time, these leaks lead to memory exhaustion and descriptor heap depletion.

### Current Behavior
- Function allocates multiple D3D12 resources during mipmap generation
- Error handling uses early `return` statements
- Early returns bypass resource cleanup
- Leaked resources:
  - Descriptor heaps for SRV/UAV
  - Pipeline State Objects (PSOs)
  - Root signatures
  - Intermediate render targets
  - Command lists/allocators
- **Memory grows unbounded** on repeated errors

### Expected Behavior
- All allocated resources properly released on all code paths
- Use RAII pattern (ComPtr, unique_ptr) for automatic cleanup
- Explicit cleanup in error paths if RAII not possible
- No memory leaks detectable by D3D12 debug layer

---

## Evidence and Code Location

### Where to Find the Issue

**File:** `src/igl/d3d12/Texture.cpp`

**Search Pattern:**
1. Locate function `Texture::generateMipmap(ICommandQueue& commandQueue)`
2. Find resource allocation sites:
   - `CreateDescriptorHeap()`
   - `CreateGraphicsPipelineState()` or `createRenderPipeline()`
   - `CreateRootSignature()`
   - Intermediate texture creation
3. Find error handling with early returns:
```cpp
if (FAILED(hr)) {
    return; // ← PROBLEM: Leaks resources allocated before this point
}
```
4. Look for `if (!result.isOk()) { return; }` patterns

**Specific Patterns to Search:**
- `FAILED(hr)` followed by `return`
- `if (!result.isOk())` followed by `return`
- Resource creation without `ComPtr` or cleanup
- Missing `Release()` calls or destructors

**Typical Leak Scenario:**
```cpp
void Texture::generateMipmap(...) {
    // Allocate resource 1
    ComPtr<ID3D12DescriptorHeap> heap;
    device->CreateDescriptorHeap(..., &heap);

    // Allocate resource 2
    ID3D12PipelineState* pso = nullptr; // ← Not using ComPtr!
    device->CreateGraphicsPipelineState(..., &pso);

    // Error occurs
    if (FAILED(hr)) {
        return; // ← Leaks 'pso', 'heap' may or may not be released
    }

    // ... more allocations
}
```

---

## Impact

**Severity:** Critical - Memory exhaustion over time
**Reliability:** Degrades stability in long-running applications
**Resource Depletion:** Descriptor heap exhaustion compounds with TASK_P0_DX12-FIND-02

**Affected Scenarios:**
- Applications using mipmap generation extensively
- Error scenarios (invalid formats, unsupported dimensions)
- Long-running sessions
- Automated testing with repeated texture operations

**Symptoms:**
- Gradual memory growth
- D3D12 debug layer reports live objects at shutdown
- Eventual out-of-memory errors
- Descriptor heap allocation failures

**Leak Severity:** 8+ leak paths identified in audit

---

## Official References

### Microsoft Documentation

1. **Resource Management** - [D3D12 Object Lifetime](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12#object-lifetime-and-cleanup)
   - "Use ComPtr for automatic reference counting"
   - "Ensure all resources released on error paths"

2. **Debug Layer Live Object Reporting** - [Detecting Leaks](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer#reporting-live-device-objects)
   ```cpp
   // Enable leak detection
   ID3D12DebugDevice* debugDevice;
   device->QueryInterface(&debugDevice);
   debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
   ```

3. **ComPtr Usage** - [Microsoft::WRL::ComPtr](https://learn.microsoft.com/en-us/cpp/cppcx/wrl/comptr-class)
   - Automatic reference counting
   - Exception-safe cleanup

### Best Practices

1. **RAII Pattern:**
   - Always use `ComPtr<T>` for COM objects
   - Use `std::unique_ptr` for custom RAII wrappers
   - Avoid raw pointers for resource ownership

2. **Error Handling:**
   - Prefer scope-based cleanup over manual Release()
   - Use guard objects for non-COM resources
   - Consider early-exit cleanup pattern

3. **Verification:**
   - Enable D3D12 debug layer during development
   - Check live objects report at shutdown
   - Use memory sanitizers (ASan) if available

---

## Implementation Guidance

### High-Level Approach

1. **Audit All Resource Allocations**
   - Identify every `Create*()` call in `generateMipmap()`
   - List all D3D12 resources allocated
   - Map out all return paths (success and error)

2. **Convert to RAII**
   - Use `ComPtr<T>` for all COM objects
   - Create RAII wrappers for non-COM resources if needed
   - Remove explicit `Release()` calls

3. **Verify All Code Paths**
   - Trace each return statement
   - Ensure no resources leak on early returns
   - Test error injection scenarios

### Detailed Steps

**Step 1: Identify All Resource Allocations**

Location: `src/igl/d3d12/Texture.cpp` - `generateMipmap()` function

Make a checklist of all resources created:
- [ ] Descriptor heaps (SRV/UAV/RTV)
- [ ] Pipeline State Objects
- [ ] Root signatures
- [ ] Intermediate textures
- [ ] Command allocators
- [ ] Command lists
- [ ] Fences
- [ ] Other?

**Step 2: Convert Raw Pointers to ComPtr**

Before (UNSAFE):
```cpp
ID3D12PipelineState* pso = nullptr;
HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
if (FAILED(hr)) {
    return; // ← LEAK: pso never released
}
// ... use pso ...
pso->Release(); // ← May not reach here if error occurs later
```

After (SAFE):
```cpp
ComPtr<ID3D12PipelineState> pso;
HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
if (FAILED(hr)) {
    return; // ✓ SAFE: pso automatically released by ComPtr destructor
}
// ... use pso ...
// No manual Release() needed
```

**Step 3: Apply to All Allocations**

Search for pattern and fix:
```cpp
// BEFORE: Leak-prone pattern
ID3D12DescriptorHeap* heap = nullptr;
ID3D12RootSignature* rootSig = nullptr;

// AFTER: RAII-safe pattern
ComPtr<ID3D12DescriptorHeap> heap;
ComPtr<ID3D12RootSignature> rootSig;
```

**Step 4: Review Error Paths**

For each `if (FAILED(hr))` or `if (!result.isOk())`:
1. Check what resources were allocated before this point
2. Verify all use `ComPtr` or equivalent RAII
3. If manual cleanup needed, add it before return

Example pattern:
```cpp
ComPtr<ID3D12Resource> resource1;
device->CreateCommittedResource(..., &resource1);

ComPtr<ID3D12Resource> resource2;
HRESULT hr = device->CreateCommittedResource(..., &resource2);
if (FAILED(hr)) {
    // resource1 automatically released by ComPtr
    return; // ✓ SAFE
}
```

**Step 5: Handle Non-COM Resources**

If there are non-COM resources (unlikely in this function, but check):
```cpp
// Example: Custom cleanup required
struct DescriptorCleanup {
    Device* device_;
    uint32_t descriptorIndex_;
    ~DescriptorCleanup() {
        if (device_) device_->freeDescriptor(descriptorIndex_);
    }
};

// Use RAII:
DescriptorCleanup cleanup{device, index};
// ... code that might return early ...
// cleanup happens automatically
```

**Step 6: Add Verification Logging**

Add telemetry to track mipmap generation:
```cpp
void Texture::generateMipmap(...) {
    IGL_LOG_DEBUG("generateMipmap: Starting for texture %p\n", this);

    // ... resource allocation and processing ...

    IGL_LOG_DEBUG("generateMipmap: Completed successfully\n");
    return;

    // On error paths:
    IGL_LOG_ERROR("generateMipmap: Failed with error %x\n", hr);
    return;
}
```

**Step 7: Test Leak Detection**

Enable D3D12 debug layer and verify no leaks:
```cpp
// In device creation (if not already present):
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

// At shutdown, check for leaks:
#ifdef _DEBUG
    ComPtr<ID3D12DebugDevice> debugDevice;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&debugDevice)))) {
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }
#endif
```

### Example Fix Pattern

Before:
```cpp
void Texture::generateMipmap(ICommandQueue& commandQueue) {
    ID3D12DescriptorHeap* srvHeap = nullptr;
    device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap)); // ← Leak risk

    ID3D12PipelineState* pso = nullptr;
    HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)); // ← Leak risk

    if (FAILED(hr)) {
        return; // ← LEAKS srvHeap and pso
    }

    // ... processing ...

    srvHeap->Release();
    pso->Release();
}
```

After:
```cpp
void Texture::generateMipmap(ICommandQueue& commandQueue) {
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap)); // ✓ Auto-cleanup

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)); // ✓ Auto-cleanup

    if (FAILED(hr)) {
        return; // ✓ SAFE: Both srvHeap and pso automatically released
    }

    // ... processing ...

    // No manual Release() needed - ComPtr handles it
}
```

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- Texture creation and manipulation tests
- Mipmap generation tests (if they exist)

**Render Sessions:**
```bash
test_all_sessions.bat
```
Sessions using mipmapped textures:
- **Textured3DCubeSession** (likely uses mipmaps)
- **TQSession** (texture quality session)
- Any session with texture sampling
- All other sessions (regression check)

### Validation Steps

1. **Leak Detection with Debug Layer**
   ```bash
   # Enable D3D12 debug layer via environment variable or code
   # Run all tests
   test_unittests.bat
   test_all_sessions.bat

   # Check output for "Live Object" reports
   # Should be zero D3D12 objects leaked
   ```

2. **Repeated Mipmap Generation**
   - Create test that generates mipmaps 1000+ times
   - Monitor memory usage (should stay flat)
   - Check debug layer for live objects

3. **Error Injection Testing**
   - Manually trigger error paths (e.g., invalid texture format)
   - Verify no leaks on error returns
   - Use debug layer to confirm

4. **Long-Running Session**
   - Run render session for extended period
   - Monitor memory usage (Task Manager, Process Explorer)
   - Should not grow over time

5. **Regression Testing**
   - All tests pass
   - Visual output unchanged
   - No new validation errors

### Memory Leak Detection

**Tools:**
- D3D12 Debug Layer: `ReportLiveDeviceObjects()`
- Visual Studio Memory Profiler
- Windows Performance Analyzer
- Application Verifier (if available)

**Expected Results:**
- Zero D3D12 live objects leaked at shutdown
- Flat memory usage over time
- No debug layer warnings about unreleased objects

---

## Success Criteria

- [ ] All resource allocations in `generateMipmap()` use `ComPtr` or RAII
- [ ] No raw pointer ownership of D3D12 COM objects
- [ ] All error paths verified to release resources
- [ ] All success paths verified to release resources
- [ ] D3D12 debug layer reports zero leaked objects
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] Memory usage stable over 1000+ mipmap generations
- [ ] No live object reports at shutdown (debug layer)
- [ ] Code review confirms no leak paths remain
- [ ] User confirms tests pass with debug layer enabled

---

## Dependencies

**Depends On:**
- None (isolated bug fix)

**Related:**
- TASK_P0_DX12-004 (HRESULT Validation) - Complement error handling improvements
- TASK_P0_DX12-FIND-02 (Descriptor Heap Overflow) - Both affect resource management

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts
   - ❌ DO NOT modify render sessions
   - ✅ Tests must pass with debug layer enabled

2. **User Confirmation Required**
   - ⚠️ Run tests with D3D12 debug layer enabled - **MANDATORY**
   - ⚠️ Verify zero live objects at shutdown
   - ⚠️ Report memory leak detection results
   - ⚠️ Wait for user confirmation

3. **Code Modification Scope**
   - ✅ Modify `Texture.cpp` - `generateMipmap()` function
   - ✅ Convert raw pointers to `ComPtr`
   - ✅ Add defensive logging
   - ❌ DO NOT change mipmap generation algorithm (unless fixing bugs)
   - ❌ DO NOT modify IGL public API

4. **Validation**
   - Must enable D3D12 debug layer for testing
   - Must verify zero leaks
   - Must test error paths explicitly

---

**Estimated Timeline:** 4-6 hours
**Risk Level:** Low-Medium (isolated to one function, well-defined fix)
**Validation Effort:** 2-3 hours (leak detection testing)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
