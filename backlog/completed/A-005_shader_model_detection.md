# A-005: Shader Model Detection Incomplete

**Priority**: LOW
**Category**: Architecture - Device Capabilities
**Estimated Effort**: 4-5 hours
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 backend detects the shader model only during device initialization and relies on a hard-coded fallback assumption. However, the detection is incomplete because:

1. **Incomplete Detection Logic**: Shader model is queried with a fixed target (SM 6.6) but doesn't implement progressive fallback if that level isn't supported
2. **No Per-Feature Validation**: Shader model support is reported as a single version number without checking if specific features are available
3. **Missing Minimum Version Enforcement**: No validation that the detected shader model meets minimum requirements for the application
4. **Incomplete Fallback Chain**: If SM 6.6 query fails, it assumes SM 6.0 without actually testing intermediate versions
5. **No Feature-Level Correlation**: Shader model detection doesn't properly correlate with the selected device feature level (FL 11.x should only support SM 5.x)

**Symptoms**:
- Device claims to support SM 6.0+ even when running on FL 11.x hardware (which only supports SM 5.x)
- Application assumes shader model capabilities that aren't available on older hardware
- Shader compilation targets SM 6.0 on hardware that doesn't support it
- No clear error messages when device shader model is insufficient
- Potential shader compilation failures on older hardware

**Impact**:
- Shader compilation failures on Feature Level 11.x hardware
- Runtime errors when using SM 6.x-only features on older GPUs
- Incompatibility with integrated GPUs that only support SM 5.x
- Difficult debugging when shaders fail on specific hardware
- No application graceful degradation based on actual shader model support

---

## 2. Root Cause Analysis

### 2.1 Current Shader Model Detection

**File**: `src/igl/d3d12/D3D12Context.cpp` (Lines 506-527)

```cpp
// Query shader model support (H-010)
// This is critical for FL11 hardware which only supports SM 5.1, not SM 6.0+
IGL_LOG_INFO("D3D12Context: Querying shader model capabilities...\n");

// Start by probing for SM 6.6 (highest commonly available)
D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };

hr = device_->CheckFeatureSupport(
    D3D12_FEATURE_SHADER_MODEL,
    &shaderModel,
    sizeof(shaderModel));

if (SUCCEEDED(hr)) {
    maxShaderModel_ = shaderModel.HighestShaderModel;
    IGL_LOG_INFO("  Detected Shader Model: %d.%d\n",
                 (maxShaderModel_ >> 4) & 0xF,
                 maxShaderModel_ & 0xF);
} else {
    // If query fails, assume SM 6.0 (DXC minimum, SM 5.x deprecated)
    maxShaderModel_ = D3D_SHADER_MODEL_6_0;
    IGL_LOG_INFO("  Shader model query failed, assuming SM 6.0 (DXC minimum)\n");
}
```

### 2.2 Why This Is Wrong

**Problem 1**: The query starts with SM 6.6 and checks if that's the maximum. `CheckFeatureSupport()` returns the highest supported shader model in the structure field, but if the API call fails, it falls back to SM 6.0 without testing intermediate versions (6.5, 6.4, 6.3, 6.2, 6.1, 6.0, 5.1).

**Problem 2**: On Feature Level 11.x hardware, the correct shader model is 5.1, not 6.0. The comment acknowledges this ("critical for FL11 hardware which only supports SM 5.1") but the code doesn't actually check the feature level before deciding on SM 6.0.

**Problem 3**: `CheckFeatureSupport()` with a feature data structure that specifies the highest shader model to query can have implementation differences. Some drivers might not support the newer SM query parameters or might have inaccurate reporting.

**Problem 4**: No validation that the detected shader model is compatible with the selected feature level. Feature Level 11.0/11.1 should use SM 5.x, while FL 12.0+ can use SM 6.x.

**Problem 5**: No checking of specific shader model features. Shader Model is a version number, but different features were added in different versions (wave intrinsics in 6.0, mesh shaders in 6.5, etc.). The code assumes all features are available if the version is reported.

**Problem 6**: Missing storage of which adapter/device supports which shader model. Multiple adapters might have different shader model capabilities, but this information is lost after device creation.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Shader Model Specification**:
   - https://learn.microsoft.com/windows/win32/direct3dhlsl/shader-model-6-1
   - https://learn.microsoft.com/windows/win32/direct3dhlsl/shader-models-and-hlsl
   - Complete shader model specification and feature support by version

2. **Feature Level and Shader Model Mapping**:
   - https://learn.microsoft.com/windows/win32/direct3d12/feature-levels
   - Detailed mapping of feature levels to supported shader models
   - FL 11.x → SM 5.1, FL 12.0+ → SM 6.0+

3. **D3D12 Feature Support Query**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport
   - https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
   - `D3D12_FEATURE_SHADER_MODEL` feature enumeration

4. **Shader Model History**:
   - https://learn.microsoft.com/windows/win32/direct3dhlsl/sm6-overview
   - SM 6.0+ features and backward compatibility

5. **Device Capability Checking Best Practices**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
   - Sample implementation of capability detection

6. **DXC Compiler Requirements**:
   - https://github.com/microsoft/DirectXShaderCompiler
   - DXC requirements for shader model support

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for shader model detection**:
```
Pattern: "maxShaderModel" OR "D3D_SHADER_MODEL" OR "Querying shader model"
Files: src/igl/d3d12/D3D12Context.cpp, src/igl/d3d12/Device.cpp
Focus: Where shader model is detected and stored
```

**Search for shader model usage**:
```
Pattern: "maxShaderModel_" OR "getShaderModel" OR "shader.*model"
Files: src/igl/d3d12/*.cpp
Focus: Where shader model is used for compilation or feature decisions
```

**Search for feature level and shader model correlation**:
```
Pattern: "featureLevel" + "shader" OR "FL.*SM" OR "D3D_FEATURE_LEVEL.*SHADER"
Files: src/igl/d3d12/D3D12Context.cpp
Focus: Whether feature level and shader model are linked
```

**Search for shader compilation**:
```
Pattern: "compile" OR "shader.*target" OR "DXCCompiler"
Files: src/igl/d3d12/ShaderModule.cpp, src/igl/d3d12/DXCCompiler.cpp
Focus: Where shader model is used for compilation target
```

### 4.2 File Locations

- `src/igl/d3d12/D3D12Context.cpp` - Shader model detection (lines 506-527)
- `src/igl/d3d12/D3D12Context.h` - Member variable `maxShaderModel_` declaration
- `src/igl/d3d12/DXCCompiler.cpp` - Uses shader model for compilation target selection
- `src/igl/d3d12/ShaderModule.cpp` - Shader compilation using shader model
- `src/igl/d3d12/Device.cpp` - May query shader model for capability reporting

### 4.3 Key Code Patterns

Look for:
- `D3D12_FEATURE_DATA_SHADER_MODEL` structure usage
- `maxShaderModel_` member variable references
- `CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, ...)` calls
- Shader compilation target selection logic
- Feature level to shader model mapping

---

## 5. Detection Strategy

### 5.1 How to Reproduce

**Scenario 1: Detect incorrect shader model on FL 11.x hardware (if available)**
```
1. Run on Feature Level 11.x hardware
2. Enable debug output to see detected shader model
3. Expected: Should detect SM 5.1
4. Current: Might incorrectly report SM 6.0 due to fallback
```

**Scenario 2: Check shader model is used correctly**
```
1. Create shader module with SM 6.0+ specific features (wave ops)
2. Run on hardware that only supports SM 5.1
3. Expected: Should fail gracefully with clear error
4. Current: May fail during compilation with cryptic error
```

**Scenario 3: Verify feature level correlation**
```
1. Log both selected feature level and shader model
2. Verify they match expected mapping:
   - FL 11.0/11.1 → SM 5.x
   - FL 12.0 → SM 6.0
   - FL 12.1 → SM 6.1
   - FL 12.2 → SM 6.6+
3. Expected: Proper correlation
4. Current: May not validate this relationship
```

### 5.2 Verification After Fix

1. **Shader Model Detected Correctly**: Run on multiple hardware configs, verify SM matches feature level
2. **Progressive Fallback Works**: Mock out SM 6.6 failure, verify lower versions are tried
3. **Feature Level Correlation**: Verify SM matches feature level selection
4. **Compilation Uses Correct Target**: Check DXC compiler receives correct SM target string
5. **Graceful Degradation**: Ensure application can work with lower shader models

---

## 6. Fix Guidance

### 6.1 Step-by-Step Implementation

#### Step 1: Add Shader Model to Device Feature Level Mapping

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: Before `createDevice()` method (around line 250)

**Current (PROBLEM)**:
```cpp
// Shader model is queried independently with hard-coded fallback
// No correlation with selected feature level
```

**Fixed (SOLUTION)**:
```cpp
// Helper: Map feature level to expected minimum shader model
auto getMinShaderModelForFeatureLevel = [](D3D_FEATURE_LEVEL fl) -> D3D_SHADER_MODEL {
    switch (fl) {
      case D3D_FEATURE_LEVEL_12_2:
        // FL 12.2 supports SM 6.6+
        return D3D_SHADER_MODEL_6_6;
      case D3D_FEATURE_LEVEL_12_1:
        // FL 12.1 supports SM 6.1 (mesh shaders, sampler feedback)
        return D3D_SHADER_MODEL_6_1;
      case D3D_FEATURE_LEVEL_12_0:
        // FL 12.0 supports SM 6.0 (wave operations)
        return D3D_SHADER_MODEL_6_0;
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
        // FL 11.x only supports SM 5.1 (HLSL Shader Model 5)
        return D3D_SHADER_MODEL_5_1;
      default:
        return D3D_SHADER_MODEL_5_1;  // Conservative fallback
    }
};

// Helper: Convert shader model to human-readable string
auto shaderModelToString = [](D3D_SHADER_MODEL sm) -> const char* {
    switch (sm) {
      case D3D_SHADER_MODEL_6_6: return "6.6";
      case D3D_SHADER_MODEL_6_5: return "6.5";
      case D3D_SHADER_MODEL_6_4: return "6.4";
      case D3D_SHADER_MODEL_6_3: return "6.3";
      case D3D_SHADER_MODEL_6_2: return "6.2";
      case D3D_SHADER_MODEL_6_1: return "6.1";
      case D3D_SHADER_MODEL_6_0: return "6.0";
      case D3D_SHADER_MODEL_5_1: return "5.1";
      default: return "Unknown";
    }
};
```

**Rationale**:
- Establishes correct shader model to feature level mapping per DirectX specs
- Provides clear expectations for which shader models are available
- Centralizes shader model string conversion

#### Step 2: Implement Progressive Shader Model Fallback

**File**: `src/igl/d3d12/D3D12Context.cpp`

**Locate**: After device creation, after feature level is selected (around line 506)

**Current (PROBLEM)**:
```cpp
// Query shader model support (H-010)
D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };

hr = device_->CheckFeatureSupport(
    D3D12_FEATURE_SHADER_MODEL,
    &shaderModel,
    sizeof(shaderModel));

if (SUCCEEDED(hr)) {
    maxShaderModel_ = shaderModel.HighestShaderModel;
    IGL_LOG_INFO("  Detected Shader Model: %d.%d\n",
                 (maxShaderModel_ >> 4) & 0xF,
                 maxShaderModel_ & 0xF);
} else {
    // Single fallback: assume SM 6.0
    maxShaderModel_ = D3D_SHADER_MODEL_6_0;
    IGL_LOG_INFO("  Shader model query failed, assuming SM 6.0\n");
}
```

**Fixed (SOLUTION)**:
```cpp
// Query shader model support with progressive fallback
IGL_LOG_INFO("D3D12Context: Querying shader model capabilities for Feature Level %d.%d...\n",
             (selectedFeatureLevel >> 4) & 0xF, selectedFeatureLevel & 0xF);

// Shader models to attempt, from highest to lowest
const D3D_SHADER_MODEL shaderModels[] = {
    D3D_SHADER_MODEL_6_6,
    D3D_SHADER_MODEL_6_5,
    D3D_SHADER_MODEL_6_4,
    D3D_SHADER_MODEL_6_3,
    D3D_SHADER_MODEL_6_2,
    D3D_SHADER_MODEL_6_1,
    D3D_SHADER_MODEL_6_0,
    D3D_SHADER_MODEL_5_1,
};

D3D_SHADER_MODEL detectedShaderModel = D3D_SHADER_MODEL_5_1;
bool shaderModelDetected = false;

// Try each shader model from highest to lowest
for (D3D_SHADER_MODEL sm : shaderModels) {
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData = { sm };
    HRESULT hr = device_->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_MODEL,
        &shaderModelData,
        sizeof(shaderModelData));

    if (SUCCEEDED(hr)) {
        detectedShaderModel = shaderModelData.HighestShaderModel;
        shaderModelDetected = true;
        IGL_LOG_INFO("  Detected Shader Model: %s\n", shaderModelToString(detectedShaderModel));
        break;  // Found highest supported, stop trying
    } else {
        IGL_LOG_INFO("  Shader Model %s not supported, trying lower version\n",
                     shaderModelToString(sm));
    }
}

if (!shaderModelDetected) {
    // Fallback based on feature level
    D3D_SHADER_MODEL minimumSM = getMinShaderModelForFeatureLevel(selectedFeatureLevel);
    IGL_LOG_WARNING("  Shader model detection failed, using minimum for Feature Level: %s\n",
                    shaderModelToString(minimumSM));
    detectedShaderModel = minimumSM;
}

// Validate shader model is appropriate for feature level
D3D_SHADER_MODEL minimumRequired = getMinShaderModelForFeatureLevel(selectedFeatureLevel);
if (detectedShaderModel < minimumRequired) {
    IGL_LOG_WARNING("  WARNING: Detected Shader Model %s is below minimum for Feature Level: %s\n",
                    shaderModelToString(detectedShaderModel),
                    shaderModelToString(minimumRequired));
}

maxShaderModel_ = detectedShaderModel;
IGL_LOG_INFO("D3D12Context: Final Shader Model selected: %s\n", shaderModelToString(maxShaderModel_));
```

**Rationale**:
- Implements progressive fallback from SM 6.6 to SM 5.1
- Actually tests each intermediate version instead of jumping from 6.6 to 6.0
- Validates detected shader model against feature level minimum requirements
- Clear logging of which versions were attempted and which was selected
- Fallback respects feature level constraints

#### Step 3: Store Shader Model in Device Header

**File**: `src/igl/d3d12/D3D12Context.h`

**Locate**: Member variables section (around line 150-200)

**Current (PROBLEM)**:
```cpp
private:
    D3D_SHADER_MODEL maxShaderModel_ = D3D_SHADER_MODEL_6_0;  // Hard-coded default
```

**Fixed (SOLUTION)**:
```cpp
private:
    // Shader model support
    D3D_SHADER_MODEL maxShaderModel_ = D3D_SHADER_MODEL_5_1;  // Conservative default
    D3D_FEATURE_LEVEL selectedFeatureLevel_ = D3D_FEATURE_LEVEL_11_0;  // Store for reference

    // Add public accessor
public:
    D3D_SHADER_MODEL getMaxShaderModel() const { return maxShaderModel_; }
    D3D_FEATURE_LEVEL getSelectedFeatureLevel() const { return selectedFeatureLevel_; }
```

**Rationale**:
- Stores both feature level and shader model for reference
- Provides safe accessors for other components
- Allows shader compilation to check capabilities

#### Step 4: Update Device Interface to Expose Shader Model

**File**: `src/igl/d3d12/Device.h` or `src/igl/Device.h`

**Locate**: Public interface methods

**Current (PROBLEM)**:
```cpp
// No public method to query shader model
// Applications must use internal D3D12Context API
```

**Fixed (SOLUTION)**:
```cpp
// In Device class or IDevice interface
public:
    /// Query maximum supported shader model for this device
    /// @return D3D_SHADER_MODEL enumeration value
    D3D_SHADER_MODEL getMaxShaderModel() const;

    /// Query selected device feature level
    /// @return D3D_FEATURE_LEVEL enumeration value
    D3D_FEATURE_LEVEL getSelectedFeatureLevel() const;
```

**Rationale**:
- Exposes shader model capability to application/shader compilation
- Allows dynamic shader target selection based on hardware
- Enables graceful feature degradation

#### Step 5: Update Shader Compilation to Use Detected Shader Model

**File**: `src/igl/d3d12/ShaderModule.cpp` or `src/igl/d3d12/DXCCompiler.cpp`

**Locate**: Shader compilation code (search for "cs_6_0", "ps_6_0", compile target)

**Current (PROBLEM)**:
```cpp
// Hard-coded SM 6.0 target
std::wstring target = L"ps_6_0";  // Always SM 6.0
```

**Fixed (SOLUTION)**:
```cpp
// Get maximum shader model from device
D3D_SHADER_MODEL maxSM = device->getMaxShaderModel();

// Select compilation target based on device capability
std::wstring target;
switch (stage) {
    case ShaderStage::Vertex:
        target = selectVertexTarget(maxSM);
        break;
    case ShaderStage::Fragment:
        target = selectFragmentTarget(maxSM);
        break;
    case ShaderStage::Compute:
        target = selectComputeTarget(maxSM);
        break;
    default:
        target = L"ps_6_0";  // Conservative fallback
}

IGL_LOG_INFO("Compiling shader with target: %ws (SM %s)\n",
             target.c_str(), shaderModelToString(maxSM));

// Helper functions
auto selectVertexTarget = [](D3D_SHADER_MODEL sm) -> std::wstring {
    if (sm >= D3D_SHADER_MODEL_6_0) return L"vs_6_0";
    if (sm >= D3D_SHADER_MODEL_5_1) return L"vs_5_1";
    return L"vs_5_1";
};

auto selectFragmentTarget = [](D3D_SHADER_MODEL sm) -> std::wstring {
    if (sm >= D3D_SHADER_MODEL_6_6) return L"ps_6_6";
    if (sm >= D3D_SHADER_MODEL_6_5) return L"ps_6_5";
    if (sm >= D3D_SHADER_MODEL_6_0) return L"ps_6_0";
    if (sm >= D3D_SHADER_MODEL_5_1) return L"ps_5_1";
    return L"ps_5_1";
};

auto selectComputeTarget = [](D3D_SHADER_MODEL sm) -> std::wstring {
    if (sm >= D3D_SHADER_MODEL_6_0) return L"cs_6_0";
    if (sm >= D3D_SHADER_MODEL_5_1) return L"cs_5_1";
    return L"cs_5_1";
};
```

**Rationale**:
- Uses actual device shader model capability instead of hard-coded assumptions
- Selects highest supported target for given stage
- Enables shader compilation to succeed on older hardware with lower targets
- Clear logging of compilation target selection

---

## 7. Testing Requirements

### 7.1 Unit Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Run shader compilation tests
./build/Debug/IGLTests.exe --gtest_filter="*Shader*"

# Run device capability tests
./build/Debug/IGLTests.exe --gtest_filter="*Capability*"
```

**Test Modifications Allowed**:
- ✅ Add shader model detection tests
- ✅ Add feature level to shader model mapping tests
- ✅ Add shader compilation target selection tests
- ✅ Mock device capabilities to test fallback logic
- ❌ **DO NOT modify cross-platform shader compilation logic**

### 7.2 Integration Tests (MANDATORY - Must Pass Before Proceeding)

```bash
# All render sessions test shader compilation
./test_all_sessions.bat

# Specific shader-heavy sessions
./build/Debug/RenderSessions.exe --session=Textured3DCube
./build/Debug/RenderSessions.exe --session=ComputeTest (if available)
```

**Expected Results**:
- Shaders compile successfully on current hardware
- Correct shader model is used for compilation
- Performance is equivalent to before fix

### 7.3 Manual Verification

1. **Shader Model Detection Logging**:
   - Run application with debug output
   - Verify logs show shader model detection
   - Verify feature level and shader model correlation

2. **Shader Compilation Success**:
   - Run any shader compilation test
   - Verify compilation completes without errors
   - Check logs show correct compilation target

3. **Hardware Compatibility** (if available):
   - Test on older hardware (FL 11.x capable)
   - Verify shader model is SM 5.1
   - Verify shaders compile successfully

---

## 8. Definition of Done

### 8.1 Completion Criteria

- [ ] Feature level to shader model mapping implemented
- [ ] Progressive shader model fallback implemented
- [ ] Shader model detection validates against feature level
- [ ] Shader compilation uses detected shader model
- [ ] Device exposes shader model capability
- [ ] All unit tests pass
- [ ] All integration tests (render sessions) pass
- [ ] Logging shows detected shader model and compilation targets
- [ ] Code review approved
- [ ] No shader compilation regressions

### 8.2 User Confirmation Required

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Shader detection logs show appropriate shader model for your hardware
2. Shaders compile successfully with detected target
3. Render sessions complete without errors
4. No unexpected performance changes

**Post in chat**:
```
A-005 Fix Complete - Ready for Review
- Shader model detection: PASS (Progressive fallback, FL-SM correlation)
- Shader compilation: PASS (Uses detected shader model)
- Unit tests: PASS (All shader model and compilation tests)
- Render sessions: PASS (All shader-heavy tests)
- Logging verification: PASS (Shader model detection shown in logs)

Awaiting confirmation to proceed.
```

---

## 9. Related Issues

### 9.1 Blocks

- (None - this is foundational for other shader-related improvements)

### 9.2 Related

- **A-004** - Feature Level Fallback (shader model must correlate with FL)
- **A-008** - Conservative Rasterization Detection (feature-level dependent)
- **A-013** - Background Shader Compilation (uses detected shader model)

---

## 10. Implementation Priority

**Priority**: P2 - Low (Device Capabilities)
**Estimated Effort**: 4-5 hours
**Risk**: Low (isolated to capability detection, doesn't affect existing shaders if they're compatible)
**Impact**: Improves compatibility with older hardware, enables graceful feature degradation based on actual capabilities

---

## 11. References

- https://learn.microsoft.com/windows/win32/direct3d12/feature-levels
- https://learn.microsoft.com/windows/win32/direct3dhlsl/shader-models-and-hlsl
- https://learn.microsoft.com/windows/win32/direct3dhlsl/sm6-overview
- https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport
- https://github.com/microsoft/DirectXShaderCompiler

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
