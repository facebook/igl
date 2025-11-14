# A-004: Feature Level Fallback Not Implemented

**Priority**: LOW
**Category**: Architecture - Device Initialization
**Estimated Effort**: 3-4 hours
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend attempts to create a device with the highest supported feature level but does not implement a graceful fallback strategy when the requested feature level is not met or device creation fails for a specific feature level. On some hardware configurations, while lower feature levels are supported, the initial attempt with a higher feature level may fail, and the application does not attempt to retry with progressively lower feature levels.

**Symptoms**:
- Device creation fails even though the hardware supports D3D12 at a lower feature level
- Application crashes or refuses to run on Feature Level 11.x hardware
- No automatic degradation to supported feature levels
- Error messages don't indicate which feature levels were attempted
- Performance assumptions based on initial feature level attempt may be incorrect

**Impact**:
- Reduced hardware compatibility for older/integrated GPUs
- Users with Feature Level 11.x hardware cannot use the application
- Support burden from incompatible hardware reports
- Lost market for devices with older graphics hardware
- No graceful feature level negotiation

---

## 2. Root Cause Analysis

### 2.1 Current Feature Level Detection

**File**: `src/igl/d3d12/D3D12Context.cpp` (Lines 310-336)

```cpp
auto getHighestFeatureLevel = [](IDXGIAdapter1* adapter) -> D3D_FEATURE_LEVEL {
    // Try creating device with FL 11.0 first (minimum required)
    Microsoft::WRL::ComPtr<ID3D12Device> tempDevice;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(tempDevice.GetAddressOf())))) {
      return static_cast<D3D_FEATURE_LEVEL>(0); // Adapter doesn't support D3D12
    }

    // Query supported feature levels (check from highest to lowest)
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelsInfo = {};
    featureLevelsInfo.NumFeatureLevels = static_cast<UINT>(std::size(featureLevels));
    featureLevelsInfo.pFeatureLevelsRequested = featureLevels;

    if (SUCCEEDED(tempDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,
                                                   &featureLevelsInfo,
                                                   sizeof(featureLevelsInfo)))) {
      return featureLevelsInfo.MaxSupportedFeatureLevel;
    }

    return D3D_FEATURE_LEVEL_11_0; // Fallback to minimum
  };
```

### 2.2 Why This Is Wrong

**Problem 1**: While `getHighestFeatureLevel()` correctly identifies the maximum supported feature level using `CheckFeatureSupport()`, the actual device creation attempt uses that single feature level:

**File**: `src/igl/d3d12/D3D12Context.cpp` (Lines 377-383)

```cpp
if (SUCCEEDED(D3D12CreateDevice(cand.Get(), featureLevel,
                                IID_PPV_ARGS(device_.GetAddressOf())))) {
    created = true;
    selectedFeatureLevel = featureLevel;
    IGL_LOG_INFO("D3D12Context: Using HW adapter (FL %s)\n", featureLevelToString(featureLevel));
    break;
}
```

**Problem 2**: If `D3D12CreateDevice()` fails for the detected feature level (which can happen on some hardware due to driver-specific issues or hardware quirks), there is no fallback mechanism. The code just continues to the next adapter instead of trying lower feature levels on the same adapter.

**Problem 3**: `CheckFeatureSupport()` reports what the driver claims is supported, but actual device creation can fail due to:
- Driver bugs or incomplete implementation
- Hardware quirks where reported capability differs from actual capability
- Driver state or system resource constraints
- GPU-specific limitations not exposed through the feature support API

**Problem 4**: No minimum feature level enforcement - if all feature level creation attempts fail, the application fails entirely instead of degrading functionality.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Feature Levels in Direct3D 12**:
   - https://learn.microsoft.com/windows/win32/direct3d12/what-is-directx-12
   - https://learn.microsoft.com/windows/win32/direct3d12/feature-levels
   - Explains feature level capabilities and selection strategy

2. **Device Creation**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice
   - Core device creation API with feature level parameter

3. **Feature Support Query**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport
   - https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
   - `D3D12_FEATURE_FEATURE_LEVELS` for capability checking

4. **Best Practices for Device Creation**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
   - Shows proper device creation with fallback patterns

5. **Backward Compatibility**:
   - https://learn.microsoft.com/windows/win32/direct3d12/feature-levels
   - Feature level compatibility guide

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for feature level setup**:
```
Pattern: "getHighestFeatureLevel" OR "featureLevels\[\]"
Files: src/igl/d3d12/D3D12Context.cpp
Focus: Feature level probing and device creation logic
```

**Search for device creation attempts**:
```
Pattern: "D3D12CreateDevice"
Files: src/igl/d3d12/D3D12Context.cpp, src/igl/d3d12/HeadlessContext.cpp
Focus: Where device is actually created
```

**Search for fallback logic**:
```
Pattern: "if (!created)" OR "fallback" OR "continue"
Files: src/igl/d3d12/D3D12Context.cpp
Focus: Current fallback strategy (adapter selection)
```

### 4.2 File Locations

- `src/igl/d3d12/D3D12Context.cpp` - Main device creation with feature level detection (lines 257-530)
- `src/igl/d3d12/D3D12Context.h` - Device class members for feature level tracking
- `src/igl/d3d12/HeadlessContext.cpp` - Alternative context that may have similar issues

### 4.3 Key Code Patterns

Look for:
- `getHighestFeatureLevel()` lambda function
- `D3D12CreateDevice()` calls with feature level parameters
- Adapter enumeration loops (factory6, EnumAdapters1, WARP)
- Feature level string conversion (`featureLevelToString`)

---

## 5. Detection Strategy

### 5.1 How to Reproduce

**Scenario 1: Test with Feature Level 11.x hardware (if available)**
```
1. Run application on hardware that only supports Feature Level 11.x
2. Observe: Device initialization should succeed
3. Expected: Application runs with reduced capabilities
4. Current: Likely fails if higher levels were attempted and failed
```

**Scenario 2: Test with driver quirk simulation**
```
1. Add test code that makes D3D12CreateDevice fail for FL 12.x
2. But succeeds for FL 11.x
3. Run application
4. Expected: Falls back to FL 11.x gracefully
5. Current: Fails or skips to next adapter
```

**Scenario 3: Instrumentation test**
```
1. Enable verbose logging in createDevice()
2. Add logging for each feature level attempt
3. Note which adapters are tried with which feature levels
4. Expected: Should see fallback attempts
5. Current: May see one attempt per adapter
```

### 5.2 Verification After Fix

1. **Feature Level Fallback Works**: Modify test to make FL 12.2 fail, verify FL 12.1 is tried
2. **Logging Shows Fallbacks**: Enable debug logging, see feature level fallback attempts
3. **Device Creation Succeeds**: Verify device is created with best available feature level
4. **No Performance Regression**: Run standard render session tests, verify timing unchanged
5. **Hardware Compatibility**: Test on both modern (FL 12.2) and older (FL 11.x) hardware

---

## 6. Fix Guidance

### 6.1 Step-by-Step Implementation

#### Step 1: Create Feature Level Fallback Function

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: Before `createDevice()` method (around line 250), add new helper function

**Current (PROBLEM)**:
```cpp
// Current approach: query highest, then single attempt at that level
auto getHighestFeatureLevel = [](IDXGIAdapter1* adapter) -> D3D_FEATURE_LEVEL {
    // ... queries reported maximum
    return featureLevelsInfo.MaxSupportedFeatureLevel;
};

// Then in createDevice():
D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(cand.Get());
if (SUCCEEDED(D3D12CreateDevice(cand.Get(), featureLevel, ...))) {
    // Success
} else {
    // Failure - move to next adapter, no retry
}
```

**Fixed (SOLUTION)**:
```cpp
// Helper function to try creating device with progressive feature level fallback
auto tryCreateDeviceWithFallback =
    [](IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL& outFeatureLevel) -> Microsoft::WRL::ComPtr<ID3D12Device> {

  // Feature levels to attempt, from highest to lowest
  const D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_12_2,
      D3D_FEATURE_LEVEL_12_1,
      D3D_FEATURE_LEVEL_12_0,
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };

  Microsoft::WRL::ComPtr<ID3D12Device> device;

  // Try each feature level from highest to lowest
  for (D3D_FEATURE_LEVEL fl : featureLevels) {
    HRESULT hr = D3D12CreateDevice(adapter, fl, IID_PPV_ARGS(device.GetAddressOf()));

    if (SUCCEEDED(hr)) {
      outFeatureLevel = fl;
      IGL_LOG_INFO("D3D12Context: Device created successfully with Feature Level %d.%d\n",
                   (fl >> 4) & 0xF, fl & 0xF);
      return device;
    } else {
      IGL_LOG_INFO("D3D12Context: Feature Level %d.%d not supported (HRESULT: 0x%08X), trying lower level\n",
                   (fl >> 4) & 0xF, fl & 0xF, hr);
    }
  }

  // All feature levels failed
  outFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
  IGL_LOG_ERROR("D3D12Context: Device creation failed for all feature levels (11.0 - 12.2)\n");
  return nullptr;
};
```

**Rationale**:
- Implements progressive fallback from highest (12.2) to lowest (11.0) feature level
- Each feature level is actually tested via device creation, not just capability query
- Comprehensive error logging shows which feature levels were attempted and why they failed
- Removes dependency on `CheckFeatureSupport()` alone, which can be inaccurate
- Allows hardware with partial feature level support to succeed

#### Step 2: Update Device Creation in Hardware Adapter Enumeration

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: Hardware adapter enumeration loop (around lines 358-384)

**Current (PROBLEM)**:
```cpp
if (factory6.Get()) {
    for (UINT i = 0;; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> cand;
      if (FAILED(factory6->EnumAdapterByGpuPreference(...))) {
        break;
      }
      // ... skip software adapter ...

      D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(cand.Get());
      if (featureLevel == static_cast<D3D_FEATURE_LEVEL>(0)) {
        continue;
      }

      if (SUCCEEDED(D3D12CreateDevice(cand.Get(), featureLevel, ...))) {
        created = true;
        selectedFeatureLevel = featureLevel;
        break;
      }
      // If create fails, loop continues to next adapter (no fallback attempt)
    }
}
```

**Fixed (SOLUTION)**:
```cpp
if (factory6.Get()) {
    for (UINT i = 0;; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> cand;
      if (FAILED(factory6->EnumAdapterByGpuPreference(...))) {
        break;
      }
      // ... skip software adapter ...

      // Try creating device with automatic feature level fallback
      D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
      auto device = tryCreateDeviceWithFallback(cand.Get(), featureLevel);

      if (device.Get() != nullptr) {
        device_ = device;
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Successfully selected HW adapter with Feature Level fallback\n");
        break;
      }
      // If all feature levels fail on this adapter, continue to next adapter
    }
}
```

**Rationale**:
- Replaces single feature level attempt with progressive fallback
- Only moves to next adapter if ALL feature levels (11.0-12.2) fail
- Maintains same adapter preference (high-performance first) while being more flexible with feature levels
- Clear logging of which feature levels were attempted

#### Step 3: Update EnumAdapters1 Fallback

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: Standard adapter enumeration loop (around lines 388-408)

**Current (PROBLEM)**:
```cpp
if (!created) {
    for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, adapter_.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
      // ... skip software adapter ...

      D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(adapter_.Get());
      if (featureLevel == static_cast<D3D_FEATURE_LEVEL>(0)) {
        continue;
      }

      if (SUCCEEDED(D3D12CreateDevice(adapter_.Get(), featureLevel, ...))) {
        created = true;
        selectedFeatureLevel = featureLevel;
        break;
      }
    }
}
```

**Fixed (SOLUTION)**:
```cpp
if (!created) {
    for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, adapter_.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
      // ... skip software adapter ...

      // Try creating device with automatic feature level fallback
      D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
      auto device = tryCreateDeviceWithFallback(adapter_.Get(), featureLevel);

      if (device.Get() != nullptr) {
        device_ = device;
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Successfully selected adapter via EnumAdapters1 with Feature Level fallback\n");
        break;
      }
    }
}
```

**Rationale**: Same as Step 2, applied to fallback enumeration method

#### Step 4: Update WARP Fallback

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: WARP adapter fallback (around lines 410-422)

**Current (PROBLEM)**:
```cpp
if (!created) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> warp;
    if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf())))) {
      D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(warp.Get());
      if (featureLevel != static_cast<D3D_FEATURE_LEVEL>(0) &&
          SUCCEEDED(D3D12CreateDevice(warp.Get(), featureLevel, ...))) {
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Using WARP adapter (FL %s)\n", featureLevelToString(featureLevel));
      }
    }
}
```

**Fixed (SOLUTION)**:
```cpp
if (!created) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> warp;
    if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf())))) {
      // Try creating WARP device with automatic feature level fallback
      D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
      auto device = tryCreateDeviceWithFallback(warp.Get(), featureLevel);

      if (device.Get() != nullptr) {
        device_ = device;
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Using WARP adapter with Feature Level fallback (FL %s)\n",
                     featureLevelToString(featureLevel));
      }
    }
}
```

**Rationale**: Ensures WARP fallback also benefits from feature level progression

#### Step 5: Update HeadlessContext

**File**: `src/igl/d3d12/HeadlessContext.cpp`

**Locate**: Device creation code (likely around line 68-100)

**Apply**: Same `tryCreateDeviceWithFallback()` function usage pattern

**Rationale**: Ensures headless context for unit tests also handles feature level fallback correctly

---

## 7. Testing Requirements

### 7.1 Unit Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Run device initialization tests specifically
./build/Debug/IGLTests.exe --gtest_filter="*DeviceInit*"

# Run adapter selection tests
./build/Debug/IGLTests.exe --gtest_filter="*Adapter*"
```

**Test Modifications Allowed**:
- ✅ Add D3D12 device feature level initialization tests
- ✅ Mock device creation to test fallback logic
- ✅ Add tests for adapter enumeration with feature level fallback
- ❌ **DO NOT modify cross-platform test logic**

### 7.2 Integration Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# All render sessions test basic device creation
./test_all_sessions.bat

# Specific critical sessions
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Expected Results**:
- Sessions initialize successfully on current hardware
- Device is created with appropriate feature level
- Logging shows feature level used

### 7.3 Manual Verification

1. **Feature Level Logging**:
   - Run any application with debug output enabled
   - Verify logs show which feature levels were attempted
   - Verify device created with highest supported level

2. **Fallback Verification** (advanced - requires test modification):
   - Temporarily modify code to make FL 12.2 fail
   - Verify FL 12.1 is attempted and succeeds
   - Check logs show fallback attempts

3. **No Regression**:
   - Run on modern hardware (FL 12.2 capable)
   - Verify same device is selected as before
   - Check performance metrics unchanged

---

## 8. Definition of Done

### 8.1 Completion Criteria

- [ ] Feature level fallback function implemented and tested
- [ ] Hardware adapter enumeration uses fallback (factory6)
- [ ] Standard adapter enumeration uses fallback (EnumAdapters1)
- [ ] WARP adapter fallback uses progression
- [ ] HeadlessContext updated with fallback
- [ ] All unit tests pass
- [ ] All integration tests (render sessions) pass
- [ ] Logging shows feature level attempts and selected level
- [ ] Code review approved
- [ ] No performance regression on modern hardware

### 8.2 User Confirmation Required

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Device initialization succeeds on your target hardware
2. Logs show appropriate feature level selected
3. Application performance is as expected (no regression)
4. No unexpected failures or crashes during standard operations

**Post in chat**:
```
A-004 Fix Complete - Ready for Review
- Device creation: PASS (Feature Level fallback implemented)
- Unit tests: PASS (all D3D12 device tests)
- Render sessions: PASS (HelloWorld, Textured3DCube)
- Logging verification: PASS (feature level fallback shown in logs)
- Performance regression: NONE

Awaiting confirmation to proceed.
```

---

## 9. Related Issues

### 9.1 Blocks

- **A-005** - Shader model detection (depends on device creation succeeding)

### 9.2 Related

- **A-001** - Hardcoded Feature Level in HeadlessContext (similar issue)
- **A-002** - Adapter Selection (runs before feature level selection)
- **A-003** - UMA Architecture Query (device must be created first)

---

## 10. Implementation Priority

**Priority**: P2 - Low (Device Initialization)
**Estimated Effort**: 3-4 hours
**Risk**: Low (isolated to device creation path, doesn't affect running application)
**Impact**: Improves hardware compatibility for older/integrated GPUs, prevents crashes on Feature Level 11.x hardware

---

## 11. References

- https://learn.microsoft.com/windows/win32/direct3d12/feature-levels
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport
- https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
