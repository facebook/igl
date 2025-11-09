# TASK_P0_DX12-004: Add Missing HRESULT Validation in Descriptor Creation

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 3-4 hours
**Status:** Open

---

## Problem Statement

The D3D12 backend contains multiple descriptor creation sites that do not validate `HRESULT` return codes. When descriptor creation fails (e.g., due to invalid parameters, out-of-memory, or unsupported formats), the code continues execution with invalid/null descriptors, leading to crashes, undefined behavior, and GPU errors downstream.

### Current Behavior
- Calls to `CreateShaderResourceView()`, `CreateUnorderedAccessView()`, `CreateRenderTargetView()`, etc. without checking return values
- Failed creation results in invalid descriptor handles
- Subsequent use of invalid descriptors causes:
  - GPU page faults
  - Device removal (`DXGI_ERROR_DEVICE_REMOVED`)
  - Silent failures (incorrect rendering)
  - Crashes in validation layer

### Expected Behavior
- All `HRESULT` return values from D3D12 API calls validated
- Failures logged with diagnostic information
- Graceful error propagation via IGL `Result` type
- No undefined behavior or crashes from invalid descriptors

---

## Evidence and Code Location

### Where to Find the Issue

**Primary Files:**
- `src/igl/d3d12/Texture.cpp`
- `src/igl/d3d12/Buffer.cpp`
- `src/igl/d3d12/RenderCommandEncoder.cpp`
- `src/igl/d3d12/ComputeCommandEncoder.cpp`

**Search Pattern - Descriptor Creation Without Validation:**

1. **Search for unchecked `CreateShaderResourceView` calls:**
```cpp
// PROBLEM: No HRESULT check
device_->CreateShaderResourceView(resource, &desc, handle);
// ← Missing: if (FAILED(hr)) ...
```

2. **Search for unchecked `CreateUnorderedAccessView` calls:**
```cpp
device_->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
// ← No error handling
```

3. **Search for unchecked `CreateRenderTargetView` calls:**
```cpp
device_->CreateRenderTargetView(resource, &desc, handle);
// ← No validation
```

4. **Search for unchecked `CreateDepthStencilView` calls:**
```cpp
device_->CreateDepthStencilView(resource, &desc, handle);
// ← No error handling
```

5. **Search for unchecked `CreateConstantBufferView` calls:**
```cpp
device_->CreateConstantBufferView(&desc, handle);
// ← No validation
```

**Search Keywords:**
- `CreateShaderResourceView` (SRV)
- `CreateUnorderedAccessView` (UAV)
- `CreateRenderTargetView` (RTV)
- `CreateDepthStencilView` (DSV)
- `CreateConstantBufferView` (CBV)
- `CreateSampler`

**What to Look For:**
- Function calls NOT assigned to a variable (no `HRESULT hr = ...`)
- No `if (FAILED(hr))` check after creation
- Direct usage of descriptor without validation

---

## Impact

**Severity:** Critical - Production stability
**Reliability:** Silent failures lead to undefined behavior
**Debugging:** Hard to diagnose GPU crashes

**Affected Code Paths:**
- All texture binding operations
- All buffer binding operations
- Render target setup
- Depth/stencil setup
- Compute shader resource binding

**Symptoms:**
- Random crashes in GPU driver
- Device removal errors
- Black screen or corrupted rendering
- Validation layer errors (if enabled)
- Non-deterministic failures

**Risk:**
- Failures only occur in edge cases (OOM, invalid formats)
- May not surface in testing but fail in production
- Hard to reproduce without specific hardware/conditions

---

## Official References

### Microsoft Documentation

1. **Error Handling in D3D12** - [HRESULT Values](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-graphics-reference-returnvalues)
   - All D3D12 methods return `HRESULT`
   - `SUCCEEDED(hr)` and `FAILED(hr)` macros for validation

2. **CreateShaderResourceView** - [Return Value](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview)
   - "This method does not return a value" ← **NOTE: Void return!**
   - Errors reported via **debug layer** only
   - Invalid parameters cause validation errors

3. **Debug Layer** - [D3D12 Debug Layer](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer)
   - Detects invalid descriptor creation
   - Errors: `D3D12 ERROR: Invalid descriptor heap`, `Invalid resource format`, etc.

4. **Device Removed Handling** - [DXGI_ERROR_DEVICE_REMOVED](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error)
   - Can result from invalid descriptor usage

### Important Note on D3D12 Descriptor Creation

**CRITICAL:** Most `Create*View()` methods in D3D12 have **VOID** return types!
- They do NOT return `HRESULT`
- Errors only detectable via:
  1. **Debug Layer** validation messages
  2. **GetDeviceRemovedReason()** after device removal
  3. Subsequent API failures

**However**, you should still validate:
- **Input parameters** before calling `Create*View()`
- **Resource validity** (non-null resource pointers)
- **Format compatibility** (check capabilities before creating view)
- **Descriptor heap validity** (non-null handle)

---

## Implementation Guidance

### High-Level Approach

Since descriptor creation methods are void (no `HRESULT`), the approach is different from typical error handling:

1. **Pre-Creation Validation**
   - Validate input parameters before calling `Create*View()`
   - Check resource pointers are non-null
   - Verify descriptor heap handles are valid
   - Check format compatibility

2. **Post-Creation Validation**
   - Enable debug layer in development
   - Check for device removal after batches of operations
   - Add assertions for critical invariants

3. **Related HRESULT Validation**
   - Validate `HRESULT` for operations that DO return errors:
     - `CreateCommittedResource()`
     - `CreateDescriptorHeap()`
     - `CreateGraphicsPipelineState()`
     - `CreateRootSignature()`
     - Command list operations

### Detailed Steps

**Step 1: Audit All Descriptor Creation Sites**

Create checklist of all `Create*View()` calls:
- [ ] `CreateShaderResourceView()` calls
- [ ] `CreateUnorderedAccessView()` calls
- [ ] `CreateRenderTargetView()` calls
- [ ] `CreateDepthStencilView()` calls
- [ ] `CreateConstantBufferView()` calls
- [ ] `CreateSampler()` calls

**Step 2: Add Pre-Creation Validation**

Before each `Create*View()` call, add validation:

```cpp
// BEFORE: Unsafe
void Texture::createSRV(...) {
    device_->CreateShaderResourceView(resource_.Get(), &desc, handle);
}

// AFTER: Validated
void Texture::createSRV(...) {
    // Validate inputs
    IGL_ASSERT_MSG(device_ != nullptr, "Device is null");
    IGL_ASSERT_MSG(resource_.Get() != nullptr, "Resource is null");
    IGL_ASSERT_MSG(handle.ptr != 0, "Descriptor handle is invalid");

    // Additional format validation
    if (!isFormatSupported(desc.Format)) {
        IGL_LOG_ERROR("D3D12: Unsupported SRV format: %d\n", desc.Format);
        // Return error via Result if possible
        return;
    }

    // Create view
    device_->CreateShaderResourceView(resource_.Get(), &desc, handle);

    // Post-creation: verify device still valid (optional)
    #ifdef _DEBUG
    if (FAILED(device_->GetDeviceRemovedReason())) {
        IGL_LOG_ERROR("D3D12: Device removed after CreateShaderResourceView\n");
    }
    #endif
}
```

**Step 3: Validate Resource Creation (HRESULT-returning operations)**

These DO return `HRESULT` and must be checked:

```cpp
// BEFORE: Unchecked
ComPtr<ID3D12Resource> resource;
device_->CreateCommittedResource(&heapProps, flags, &desc, state, nullptr,
                                  IID_PPV_ARGS(&resource));

// AFTER: Validated
ComPtr<ID3D12Resource> resource;
HRESULT hr = device_->CreateCommittedResource(&heapProps, flags, &desc, state, nullptr,
                                               IID_PPV_ARGS(&resource));
if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12: CreateCommittedResource failed with HRESULT 0x%08X\n", hr);
    // Propagate error via IGL Result
    if (outResult) {
        *outResult = Result(Result::Code::RuntimeError, "Failed to create D3D12 resource");
    }
    return nullptr;
}
IGL_ASSERT_MSG(resource != nullptr, "CreateCommittedResource succeeded but returned null");
```

**Step 4: Add Descriptor Heap Validation**

```cpp
// Validate descriptor heap creation
D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
heapDesc.NumDescriptors = 1024;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

ComPtr<ID3D12DescriptorHeap> heap;
HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12: CreateDescriptorHeap failed with HRESULT 0x%08X\n", hr);
    // Handle error...
    return;
}
IGL_ASSERT_MSG(heap != nullptr, "CreateDescriptorHeap succeeded but returned null");
```

**Step 5: Check Device Removed Reason**

Add periodic device health checks (optional, for long-running operations):

```cpp
// After batch of operations, check device health
HRESULT deviceRemovedReason = device_->GetDeviceRemovedReason();
if (FAILED(deviceRemovedReason)) {
    IGL_LOG_ERROR("D3D12: Device removed with reason 0x%08X\n", deviceRemovedReason);
    // Propagate error...
}
```

**Step 6: Enable Debug Layer in Development Builds**

Ensure debug layer enabled to catch descriptor validation errors:

```cpp
#ifdef _DEBUG
void Device::enableDebugLayer() {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();

        // Optional: Enable GPU-based validation (slower but catches more errors)
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1))) {
            debugController1->SetEnableGPUBasedValidation(TRUE);
        }
    }
}
#endif
```

**Step 7: Add Format Capability Checking (Recommended)**

Before creating views, verify format support:

```cpp
bool Device::isFormatSupportedForSRV(DXGI_FORMAT format) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { format };
    HRESULT hr = device_->CheckFeatureSupport(
        D3D12_FEATURE_FORMAT_SUPPORT,
        &formatSupport,
        sizeof(formatSupport)
    );

    if (FAILED(hr)) {
        return false;
    }

    return (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
}
```

### Priority Sites for Validation

**High Priority (Most Critical):**
1. Texture SRV creation - `Texture.cpp`
2. Render target RTV creation - `Texture.cpp`, `RenderCommandEncoder.cpp`
3. Buffer UAV creation - `Buffer.cpp`
4. Depth/stencil DSV creation - `Texture.cpp`

**Medium Priority:**
5. Constant buffer CBV creation - `RenderCommandEncoder.cpp`, `ComputeCommandEncoder.cpp`
6. Sampler creation - `Sampler.cpp` or similar

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- All resource creation tests
- Descriptor creation tests
- Error handling tests (if they exist)

**Render Sessions:**
```bash
test_all_sessions.bat
```
All sessions must pass:
- BasicFramebufferSession
- Textured3DCubeSession (SRV creation)
- MRTSession (multiple RTV creation)
- DrawInstancedSession
- All compute sessions (UAV creation)
- All other sessions

### Validation Steps

1. **Enable Debug Layer Testing**
   ```bash
   # Set environment variable or modify code to enable debug layer
   # Run all tests
   test_unittests.bat
   test_all_sessions.bat

   # Check for validation errors in output
   # Should be ZERO errors related to invalid descriptors
   ```

2. **Error Injection Testing**
   - Manually test error paths (if possible):
     - Pass invalid format to descriptor creation
     - Pass null resource pointer
     - Pass invalid descriptor handle
   - Verify error handling works correctly (logs error, doesn't crash)

3. **Regression Testing**
   - All existing tests pass
   - No new errors introduced
   - Visual output unchanged

4. **Device Removal Testing**
   - Run long sessions (60+ seconds)
   - Check for device removal errors
   - Should complete without device removal

---

## Success Criteria

- [ ] All `Create*View()` calls have pre-validation of inputs
- [ ] All resource creation calls (`CreateCommittedResource`, etc.) validate `HRESULT`
- [ ] All descriptor heap creation validates `HRESULT`
- [ ] All pipeline/root signature creation validates `HRESULT`
- [ ] Null pointer checks before calling `Create*View()`
- [ ] Format compatibility checks (optional but recommended)
- [ ] Debug layer enabled in debug builds
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] Zero validation errors from debug layer
- [ ] No device removal errors
- [ ] User confirms tests pass with debug layer enabled

---

## Dependencies

**Depends On:**
- None (isolated improvement)

**Related:**
- TASK_P0_DX12-003 (Resource Leaks) - Complement error handling improvements
- TASK_P1_DX12-006 (Device Removal Exceptions) - Related error handling

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts
   - ❌ DO NOT modify render sessions
   - ✅ Tests must pass with debug layer enabled

2. **User Confirmation Required**
   - ⚠️ Run tests with D3D12 debug layer enabled - **MANDATORY**
   - ⚠️ Report any validation errors found
   - ⚠️ Wait for user confirmation

3. **Code Modification Scope**
   - ✅ Modify `Texture.cpp`, `Buffer.cpp`, `Device.cpp`
   - ✅ Modify encoder files for descriptor binding validation
   - ✅ Add validation helpers
   - ❌ DO NOT change descriptor creation logic (only add validation)
   - ❌ DO NOT modify IGL public API

4. **Backward Compatibility**
   - New validation must not break existing functionality
   - Error handling should be graceful
   - Log errors but don't crash unless critical

---

**Estimated Timeline:** 3-4 hours
**Risk Level:** Low (additive change, improves robustness)
**Validation Effort:** 2-3 hours (debug layer testing)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
