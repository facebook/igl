# A-008: Conservative Rasterization Support Not Queried

**Priority**: LOW
**Category**: Architecture - Rendering Features
**Estimated Effort**: 3-4 hours
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend queries conservative rasterization support during device initialization but does not properly expose or validate this capability. Conservative rasterization is a device feature that affects pixel coverage calculation and is feature-level dependent (FL 12.0+), but the current implementation:

1. **Silent Detection**: Conservative rasterization tier is queried but only logged, not stored or exposed to the application
2. **No Capability Exposure**: Applications cannot query whether conservative rasterization is available before attempting to use it
3. **No Feature-Level Correlation**: Doesn't validate that conservative rasterization is only attempted on FL 12.0+ hardware
4. **Incomplete Feature Tiers**: Conservative rasterization has multiple tiers (disabled, tier 1, tier 2, tier 3) but only the raw tier number is logged
5. **No Graceful Fallback**: If conservative rasterization is unsupported, there's no mechanism to disable or degrade the feature

**Symptoms**:
- Application cannot determine if conservative rasterization is supported
- Attempting to use conservative rasterization without checking device capability
- No clear error messages if rasterization mode is unsupported
- Performance queries cannot account for conservative rasterization overhead
- Debugging difficult when conservative rasterization fails silently

**Impact**:
- Applications cannot make informed decisions about rasterization modes
- Potential failures or undefined behavior on hardware without conservative rasterization
- No granular control over rasterization quality vs. performance tradeoff
- Difficult to implement feature-dependent shader compilation
- Missed opportunity for rendering optimizations

---

## 2. Root Cause Analysis

### 2.1 Current Conservative Rasterization Detection

**File**: `src/igl/d3d12/Device.cpp` (Lines 61-97)

```cpp
void Device::validateDeviceLimits() {
  auto* device = ctx_->getDevice();
  if (!device) {
    IGL_LOG_ERROR("Device::validateDeviceLimits: D3D12 device is null\n");
    return;
  }

  IGL_LOG_INFO("=== D3D12 Device Capabilities and Limits Validation ===\n");

  // Query D3D12_FEATURE_D3D12_OPTIONS for resource binding tier and other capabilities
  HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                                           &deviceOptions_, sizeof(deviceOptions_));

  if (SUCCEEDED(hr)) {
    // ... other capability logging ...
    IGL_LOG_INFO("  Conservative Rasterization Tier: %d\n",
                 deviceOptions_.ConservativeRasterizationTier);
  } else {
    IGL_LOG_ERROR("  Failed to query D3D12_FEATURE_D3D12_OPTIONS (HRESULT: 0x%08X)\n", hr);
  }
  // ... rest of validation ...
}
```

### 2.2 Why This Is Wrong

**Problem 1**: Conservative rasterization tier is queried and logged but never stored in a member variable or exposed through any public interface. The information is immediately lost after logging.

**Problem 2**: The tier is reported as a raw integer (0, 1, 2, or 3) without explanation of what each tier means:
- Tier 0: Conservative rasterization not supported
- Tier 1: Underestimate and overestimate modes only
- Tier 2: Underestimate, overestimate, and guaranteed conservative modes
- Tier 3: Advanced features (post-snap exclusive triangle rasterization)

**Problem 3**: No validation that conservative rasterization is only used on FL 12.0+ hardware. Feature Level 11.x does not support conservative rasterization, but the code doesn't check this relationship.

**Problem 4**: No method for the application to query this capability. Other components cannot determine if conservative rasterization is available before attempting to set rasterizer states.

**Problem 5**: If tier 0 is detected (not supported), there's no fallback mechanism or error handling. Applications are expected to gracefully degrade but have no way to detect the situation.

**Problem 6**: No correlation with the selected feature level. Conservative rasterization is only available on FL 12.0+, so if FL 11.x is selected, the tier should always be 0.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Conservative Rasterization Overview**:
   - https://learn.microsoft.com/windows/win32/direct3d12/conservative-rasterization
   - Complete guide to conservative rasterization modes and tiers

2. **Rasterizer State Structure**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc
   - `D3D12_CONSERVATIVE_RASTERIZATION_MODE` field in rasterizer description

3. **Device Feature Querying**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_conservative_rasterization_tier
   - `D3D12_CONSERVATIVE_RASTERIZATION_TIER` enumeration

4. **Feature Level Requirements**:
   - https://learn.microsoft.com/windows/win32/direct3d12/feature-levels
   - Conservative rasterization requires Feature Level 12.0 or higher

5. **Best Practices**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
   - Sample capability detection patterns

6. **Rasterization Techniques**:
   - https://learn.microsoft.com/windows/win32/direct3d12/conservative-rasterization
   - Performance implications and use cases

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for conservative rasterization detection**:
```
Pattern: "ConservativeRasterization" OR "conservativeRasterization"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/D3D12Context.cpp
Focus: Where capability is queried and stored
```

**Search for rasterizer state**:
```
Pattern: "RasterizerState" OR "D3D12_RASTERIZER_DESC" OR "rasterizer"
Files: src/igl/d3d12/*.cpp
Focus: Where rasterization mode is set
```

**Search for device capabilities**:
```
Pattern: "validateDeviceLimits" OR "deviceOptions_"
Files: src/igl/d3d12/Device.cpp
Focus: Where device capabilities are validated
```

**Search for feature-level checks**:
```
Pattern: "D3D_FEATURE_LEVEL" + "12"
Files: src/igl/d3d12/*.cpp
Focus: Where feature level-dependent features are handled
```

### 4.2 File Locations

- `src/igl/d3d12/Device.cpp` - Conservative rasterization detection (lines 61-97)
- `src/igl/d3d12/Device.h` - Member variables and public interface
- `src/igl/d3d12/RenderPipelineState.cpp` - Rasterizer state setup
- `src/igl/d3d12/D3D12Context.cpp` - Device context with feature level info

### 4.3 Key Code Patterns

Look for:
- `D3D12_FEATURE_D3D12_OPTIONS` query
- `deviceOptions_.ConservativeRasterizationTier` usage
- `D3D12_CONSERVATIVE_RASTERIZATION_TIER` enumeration
- `D3D12_RASTERIZER_DESC` construction
- `D3D12_CONSERVATIVE_RASTERIZATION_MODE` usage

---

## 5. Detection Strategy

### 5.1 How to Reproduce

**Scenario 1: Verify detection on current hardware**
```
1. Run application with debug output
2. Look for: "Conservative Rasterization Tier: X" in logs
3. Expected: Tier number appropriate for hardware (0 on FL 11.x, 1+ on FL 12.0+)
4. Current: May show tier but no way to query it programmatically
```

**Scenario 2: Test on Feature Level 11.x (if available)**
```
1. Force device to FL 11.x
2. Check logged conservative rasterization tier
3. Expected: Should be 0 (not supported)
4. Current: May incorrectly report higher tier
```

**Scenario 3: Check capability query availability**
```
1. Try to find public method to query conservative rasterization
2. Expected: Public API available
3. Current: No public interface exists
```

### 5.2 Verification After Fix

1. **Capability Query Works**: Call public method to get conservative rasterization tier
2. **Feature-Level Validation**: Verify tier 0 on FL 11.x, appropriate tier on FL 12.0+
3. **Logging Shows Full Information**: Logs explain what each tier means, not just the number
4. **Fallback Works**: Application can gracefully disable conservative rasterization if not supported
5. **No Regression**: Existing rasterization tests continue to pass

---

## 6. Fix Guidance

### 6.1 Step-by-Step Implementation

#### Step 1: Create Conservative Rasterization Enumeration and Helpers

**File**: `src/igl/d3d12/Device.h`

**Locate**: Before class definition or in Common.h

**Current (PROBLEM)**:
```cpp
// No enumeration or helper for conservative rasterization tiers
// Tier is just reported as integer
```

**Fixed (SOLUTION)**:
```cpp
// In Device.h or Common.h, add:

/// Conservative rasterization tier support
enum class ConservativeRasterizationTier {
  NotSupported = 0,      // Tier 0: Feature not available
  Tier1 = 1,             // Tier 1: Underestimate and overestimate modes
  Tier2 = 2,             // Tier 2: + Guaranteed conservative mode
  Tier3 = 3,             // Tier 3: + Post-snap exclusive triangle
};

/// Convert D3D12 tier to IGL tier
inline ConservativeRasterizationTier toConservativeRasterizationTier(
    D3D12_CONSERVATIVE_RASTERIZATION_TIER d3dTier) {
  switch (d3dTier) {
    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_1:
      return ConservativeRasterizationTier::Tier1;
    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_2:
      return ConservativeRasterizationTier::Tier2;
    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_3:
      return ConservativeRasterizationTier::Tier3;
    default:
      return ConservativeRasterizationTier::NotSupported;
  }
}

/// Get human-readable tier description
inline const char* conservativeRasterizationTierToString(
    ConservativeRasterizationTier tier) {
  switch (tier) {
    case ConservativeRasterizationTier::NotSupported:
      return "NotSupported";
    case ConservativeRasterizationTier::Tier1:
      return "Tier1 (Underestimate/Overestimate)";
    case ConservativeRasterizationTier::Tier2:
      return "Tier2 (+ Guaranteed Conservative)";
    case ConservativeRasterizationTier::Tier3:
      return "Tier3 (+ Post-Snap Exclusive)";
    default:
      return "Unknown";
  }
}
```

**Rationale**:
- Provides type-safe representation of conservative rasterization capabilities
- Clear enumeration of what each tier supports
- Helper functions for conversion and logging
- Centralizes capability definitions

#### Step 2: Store Conservative Rasterization in Device

**File**: `src/igl/d3d12/Device.h`

**Locate**: Member variables section (after deviceOptions_ declaration)

**Current (PROBLEM)**:
```cpp
private:
    D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
    // Conservative rasterization info is in deviceOptions_ but never exposed
```

**Fixed (SOLUTION)**:
```cpp
private:
    D3D12_FEATURE_DATA_D3D12_OPTIONS deviceOptions_ = {};
    ConservativeRasterizationTier conservativeRasterizationTier_ =
        ConservativeRasterizationTier::NotSupported;

public:
    /// Get maximum supported conservative rasterization tier
    /// @return Tier level (0=NotSupported, 1-3=Tier1-3)
    ConservativeRasterizationTier getConservativeRasterizationTier() const {
        return conservativeRasterizationTier_;
    }

    /// Check if conservative rasterization is supported
    /// @return true if Tier 1 or higher is available
    bool supportsConservativeRasterization() const {
        return conservativeRasterizationTier_ != ConservativeRasterizationTier::NotSupported;
    }

    /// Check if guaranteed conservative mode is supported (Tier 2+)
    /// @return true if Tier 2 or higher is available
    bool supportsGuaranteedConservativeRasterization() const {
        return conservativeRasterizationTier_ >= ConservativeRasterizationTier::Tier2;
    }
```

**Rationale**:
- Stores detected tier in member variable for later access
- Provides multiple query methods for different use cases
- Type-safe access to conservative rasterization capability
- Clear intent with boolean helpers

#### Step 3: Update validateDeviceLimits to Store Conservative Rasterization

**File**: `src/igl/d3d12/Device.cpp`

**Locate**: validateDeviceLimits() method (lines 61-97), conservative rasterization logging section

**Current (PROBLEM)**:
```cpp
if (SUCCEEDED(hr)) {
    // ... other logging ...
    IGL_LOG_INFO("  Conservative Rasterization Tier: %d\n",
                 deviceOptions_.ConservativeRasterizationTier);
} else {
    IGL_LOG_ERROR("  Failed to query D3D12_FEATURE_D3D12_OPTIONS\n");
}
```

**Fixed (SOLUTION)**:
```cpp
if (SUCCEEDED(hr)) {
    // ... other logging ...

    // Parse conservative rasterization capability
    conservativeRasterizationTier_ =
        toConservativeRasterizationTier(deviceOptions_.ConservativeRasterizationTier);

    IGL_LOG_INFO("  Conservative Rasterization: %s\n",
                 conservativeRasterizationTierToString(conservativeRasterizationTier_));

    if (supportsConservativeRasterization()) {
      IGL_LOG_INFO("    - Conservative rasterization is SUPPORTED\n");
      if (supportsGuaranteedConservativeRasterization()) {
        IGL_LOG_INFO("    - Guaranteed conservative mode is AVAILABLE\n");
      }
    } else {
      IGL_LOG_INFO("    - Conservative rasterization is NOT SUPPORTED\n");
    }
} else {
    IGL_LOG_ERROR("  Failed to query D3D12_FEATURE_D3D12_OPTIONS\n");
    conservativeRasterizationTier_ = ConservativeRasterizationTier::NotSupported;
}

// Validate conservative rasterization against feature level
if (supportsConservativeRasterization()) {
    // Conservative rasterization requires FL 12.0+
    if (ctx_->getSelectedFeatureLevel() < D3D_FEATURE_LEVEL_12_0) {
      IGL_LOG_WARNING("  WARNING: Conservative rasterization supported but Feature Level is < 12.0\n");
    }
}
```

**Rationale**:
- Actually stores detected tier instead of just logging it
- Provides clear, descriptive logging of what each tier means
- Validates feature level correlation
- Enables other code to query the capability

#### Step 4: Add Validation in Rasterizer State Creation

**File**: `src/igl/d3d12/RenderPipelineState.cpp`

**Locate**: Where rasterizer state is created with conservative rasterization mode

**Current (PROBLEM)**:
```cpp
// If using conservative rasterization, no check for device support
D3D12_RASTERIZER_DESC rasterizerDesc = {};
rasterizerDesc.ConservativeRasterization = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
// ... no validation that it's supported ...
```

**Fixed (SOLUTION)**:
```cpp
// In RenderPipelineState creation, validate conservative rasterization
D3D12_RASTERIZER_DESC rasterizerDesc = {};

if (desc.conservativeRasterizationMode != ConservativeRasterizationMode::Disabled) {
    // Check if device supports conservative rasterization
    if (!device->supportsConservativeRasterization()) {
      IGL_LOG_WARNING("RenderPipelineState: Conservative rasterization requested "
                      "but device does not support it. Using standard rasterization.\n");
      rasterizerDesc.ConservativeRasterization = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    } else if (desc.conservativeRasterizationMode == ConservativeRasterizationMode::Guaranteed) {
      // Check if device supports guaranteed conservative (Tier 2+)
      if (!device->supportsGuaranteedConservativeRasterization()) {
        IGL_LOG_WARNING("RenderPipelineState: Guaranteed conservative rasterization requested "
                        "but device only supports Tier 1. Using standard conservative.\n");
        rasterizerDesc.ConservativeRasterization = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
      } else {
        rasterizerDesc.ConservativeRasterization = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
      }
    } else {
      rasterizerDesc.ConservativeRasterization = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
    }
} else {
    rasterizerDesc.ConservativeRasterization = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}
```

**Rationale**:
- Validates conservative rasterization before use
- Gracefully degrades if feature is unsupported
- Clear logging of fallback decision
- Prevents errors from unsupported rasterization modes

#### Step 5: Expose in Device Interface

**File**: `src/igl/Device.h` (if cross-platform) or keep in `src/igl/d3d12/Device.h`

**Locate**: IDevice interface or D3D12 Device class public methods

**Current (PROBLEM)**:
```cpp
// No interface for querying conservative rasterization
```

**Fixed (SOLUTION)**:
```cpp
// In D3D12 Device class public section, ensure methods are available:
public:
    /// Get maximum supported conservative rasterization tier
    ConservativeRasterizationTier getConservativeRasterizationTier() const;

    /// Check if conservative rasterization is supported
    bool supportsConservativeRasterization() const;

    /// Check if guaranteed conservative mode is supported
    bool supportsGuaranteedConservativeRasterization() const;
```

**Rationale**:
- Exposes capability to application and shader compilation code
- Enables dynamic shader compilation based on available rasterization modes
- Clear public API for capability querying

---

## 7. Testing Requirements

### 7.1 Unit Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Run device capability tests
./build/Debug/IGLTests.exe --gtest_filter="*Capability*"

# Run rasterization tests
./build/Debug/IGLTests.exe --gtest_filter="*Rasteriz*"
```

**Test Modifications Allowed**:
- ✅ Add conservative rasterization capability detection tests
- ✅ Add conservative rasterization tier validation tests
- ✅ Add feature-level correlation tests
- ✅ Mock device to test graceful degradation
- ❌ **DO NOT modify cross-platform rasterization logic**

### 7.2 Integration Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# All render sessions test rasterization
./test_all_sessions.bat

# Specific rasterization-relevant sessions
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Expected Results**:
- Sessions render successfully
- No console errors about unsupported rasterization
- Logging shows conservative rasterization capability

### 7.3 Manual Verification

1. **Capability Detection Logging**:
   - Run application with debug output
   - Look for conservative rasterization tier information
   - Verify it matches hardware capability

2. **Feature-Level Validation**:
   - If possible to force FL 11.x, verify tier shows as not supported
   - On FL 12.0+ hardware, verify appropriate tier is reported

3. **Graceful Degradation**:
   - No errors if conservative rasterization is unsupported
   - Application renders correctly with standard rasterization fallback

---

## 8. Definition of Done

### 8.1 Completion Criteria

- [ ] Conservative rasterization enumeration created
- [ ] Capability stored in Device member variable
- [ ] Public query methods implemented
- [ ] validateDeviceLimits() stores and logs capability
- [ ] Feature-level validation implemented
- [ ] Rasterizer state creation validates capability
- [ ] All unit tests pass
- [ ] All integration tests (render sessions) pass
- [ ] Logging shows clear tier information
- [ ] Code review approved
- [ ] No rasterization regressions

### 8.2 User Confirmation Required

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Conservative rasterization capability is correctly detected and logged
2. Render sessions complete without errors
3. No performance regression
4. Logging clearly shows tier information

**Post in chat**:
```
A-008 Fix Complete - Ready for Review
- Conservative rasterization detection: PASS (Properly stored and exposed)
- Capability query API: PASS (Public methods available)
- Feature-level validation: PASS (Tier validates against FL)
- Unit tests: PASS (All capability tests)
- Render sessions: PASS (All sessions render correctly)
- Logging verification: PASS (Tier information shown in logs)

Awaiting confirmation to proceed.
```

---

## 9. Related Issues

### 9.1 Blocks

- (None - this is detection/exposure of existing capability)

### 9.2 Related

- **A-004** - Feature Level Fallback (conservative rasterization depends on FL 12.0+)
- **A-005** - Shader Model Detection (both are device capability queries)
- **A-012** - Video Memory Budget (another capability query)

---

## 10. Implementation Priority

**Priority**: P2 - Low (Rendering Features)
**Estimated Effort**: 3-4 hours
**Risk**: Low (isolated to capability detection and rasterizer state, doesn't affect existing non-conservative rasterization)
**Impact**: Enables applications to make informed decisions about rasterization modes, improves compatibility with lower-tier hardware

---

## 11. References

- https://learn.microsoft.com/windows/win32/direct3d12/conservative-rasterization
- https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc
- https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_conservative_rasterization_tier
- https://learn.microsoft.com/windows/win32/direct3d12/feature-levels

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
