# TASK_P1_DX12-006: Replace Device Removal Exceptions with Result Codes

**Priority:** P1 - High
**Estimated Effort:** 6-8 hours
**Status:** Open

---

## Problem Statement

The D3D12 backend throws C++ exceptions on device removal (`DXGI_ERROR_DEVICE_REMOVED`), violating IGL's error handling convention of using `Result` return codes. Uncaught exceptions crash the application instead of allowing graceful error handling and recovery.

### Current Behavior
- Device removal detected in various code paths
- Code throws exceptions: `throw std::runtime_error("Device removed")`
- Application crashes if exception not caught
- Violates IGL convention: **NO EXCEPTIONS**

### Expected Behavior
- Device removal detected and returned via `Result` code
- Error propagated to caller through return values
- Application can handle error gracefully
- Consistent with Vulkan/OpenGL backends

---

## Evidence and Code Location

**Search Pattern:**
- Search for `throw` statements in D3D12 code
- Search for `DXGI_ERROR_DEVICE_REMOVED` handling
- Search for `GetDeviceRemovedReason()` calls
- Look for exception handling (`try`/`catch`) blocks

**Common Locations:**
- `src/igl/d3d12/Device.cpp`
- `src/igl/d3d12/CommandQueue.cpp`
- `src/igl/d3d12/SwapChain.cpp`
- Any function checking `HRESULT` for device removal

**Typical Pattern to Find:**
```cpp
if (hr == DXGI_ERROR_DEVICE_REMOVED) {
    throw std::runtime_error("Device removed");  // ← REPLACE WITH Result
}
```

---

## Impact

**Severity:** High - Crashes application
**Reliability:** Prevents graceful error handling
**API Contract:** Violates IGL design principles

**Affected Code Paths:**
- Present operations
- Command submission
- Resource creation after device loss
- Any HRESULT checking

---

## Official References

### Microsoft Documentation

1. **Device Removal Handling** - [DXGI_ERROR_DEVICE_REMOVED](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error)
2. **GetDeviceRemovedReason** - [ID3D12Device::GetDeviceRemovedReason](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getdeviceremovedreason)
   - Returns reason: `DXGI_ERROR_DEVICE_HUNG`, `DXGI_ERROR_DEVICE_RESET`, etc.

### IGL Convention
- All errors returned via `Result` type
- No exceptions thrown from IGL implementation
- Caller responsible for checking `Result::isOk()`

---

## Implementation Guidance

### High-Level Approach

1. **Find All throw Statements**
   - Search codebase for `throw` in d3d12 folder
   - Categorize by error type

2. **Replace with Result Returns**
   - Convert `throw` to `return Result(Result::Code::RuntimeError, message)`
   - Update function signatures to return `Result` if needed

3. **Propagate Results Upward**
   - Functions that call throwing functions must check `Result`
   - Propagate errors to API boundary

4. **Add Device Removal Detection**
   - Check `GetDeviceRemovedReason()` after `HRESULT` failures
   - Include removal reason in error message

### Detailed Steps

**Step 1: Search for All Exceptions**
```bash
grep -r "throw" src/igl/d3d12/
```

**Step 2: Convert Device Removal Exceptions**

Before:
```cpp
void CommandQueue::submit(...) {
    HRESULT hr = ...;
    if (hr == DXGI_ERROR_DEVICE_REMOVED) {
        throw std::runtime_error("Device removed");
    }
}
```

After:
```cpp
Result CommandQueue::submit(...) {
    HRESULT hr = ...;
    if (hr == DXGI_ERROR_DEVICE_REMOVED) {
        HRESULT reason = device_->GetDeviceRemovedReason();
        return Result(Result::Code::RuntimeError,
                      "Device removed, reason: 0x" + std::to_string(reason));
    }
    return Result();  // Success
}
```

**Step 3: Update Function Signatures**
- Change `void` functions to return `Result`
- Change functions returning values to use out-parameters + `Result`

**Step 4: Update Callers**
- Check returned `Result`
- Propagate errors upward

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

All tests must pass without exceptions.

### Validation Steps

1. **No Exceptions Thrown**
   - Run all tests
   - Verify no uncaught exceptions
   - Check no `throw` statements remain in d3d12 code

2. **Error Propagation**
   - Verify errors returned via `Result`
   - Check error messages contain useful information

3. **Device Removal Simulation** (if possible)
   - Trigger device removal (TDR, driver reset)
   - Verify graceful error handling (no crash)

---

## Success Criteria

- [ ] All `throw` statements removed from d3d12 code
- [ ] Device removal handled via `Result` returns
- [ ] Function signatures updated as needed
- [ ] All callers check `Result` codes
- [ ] Error messages include `GetDeviceRemovedReason()` information
- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] No uncaught exceptions in any test
- [ ] User confirms no exceptions, graceful error handling

---

## Dependencies

**Related:**
- TASK_P0_DX12-004 (HRESULT Validation) - Complement error handling
- TASK_P0_DX12-005 (Upload Buffer Sync) - Related synchronization errors

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **User Confirmation Required:** ⚠️ Wait for confirmation
3. **Code Scope:** ✅ Modify error handling in d3d12 backend only
4. **API Contract:** ❌ DO NOT change IGL public API (only implementation)

---

**Estimated Timeline:** 6-8 hours
**Risk Level:** Medium (affects error handling throughout backend)
**Validation Effort:** 2-3 hours

---

*Task Created: 2025-11-08*
