# E-001: Missing DXC Version/Availability Check

**Severity:** Critical
**Category:** Toolchain & Compilation
**Status:** Open
**Related Issues:** E-002 (Shader model validation), H-009 (Shader model detection)

---

## Problem Statement

The DXC compiler initialization does not verify the DXC runtime version or check for minimum required version support. This creates a **silent failure mode** where applications compile successfully in development but fail at runtime on systems with outdated Windows SDKs or older DXC versions.

**The Danger:**
- No version checking before using DXC features
- DXC API evolved significantly with breaking changes between versions
- Runtime failures produce cryptic COM errors (0x80004001, 0x80070057)
- Customer deployments fail on older Windows 10 builds
- SM 6.6+ features used without verifying DXC 1.6+ support

**Example Silent Failure:**
```cpp
// Initialize DXC
auto result = dxcCompiler.initialize();  // ✓ Succeeds (DXC 1.4 available)

// Compile shader with SM 6.5 features
result = dxcCompiler.compile(source, "main", "ps_6_5", ...);
// ❌ FAILS: DXC 1.4 doesn't support SM 6.5 (requires DXC 1.5+)
// Error: "error: invalid profile 'ps_6_5'"
```

---

## Root Cause Analysis

### Current Implementation (No Version Check):

**File:** `src/igl/d3d12/DXCCompiler.cpp:18-59`

```cpp
Result DXCCompiler::initialize() {
  if (initialized_) {
    return Result();
  }

  IGL_LOG_INFO("DXCCompiler: Initializing DXC compiler...\n");

  // Create DXC utils
  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create DxcUtils: 0x%08X\n", hr);
    return Result(Result::Code::RuntimeError, "Failed to create DxcUtils");
  }

  // Create DXC compiler
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create DxcCompiler: 0x%08X\n", hr);
    return Result(Result::Code::RuntimeError, "Failed to create DxcCompiler");
  }

  // ❌ NO VERSION CHECK!
  // DXC created successfully, but we don't know which version
  // Could be DXC 1.0 (2016) or DXC 1.7 (2023) - vastly different capabilities!

  initialized_ = true;
  IGL_LOG_INFO("DXCCompiler: Initialization successful (Shader Model 6.0+ enabled)\n");
  // ❌ MISLEADING LOG: SM 6.0+ may not actually be supported!

  return Result();
}
```

### Why This Is Wrong:

**DXC Version Evolution (Breaking Changes):**

| DXC Version | Release | DXIL Version | Shader Model | Breaking Changes |
|-------------|---------|--------------|--------------|------------------|
| 1.0 | 2016 | 1.0 | 6.0 | Initial release |
| 1.4 | 2019 | 1.4 | 6.4 | API changes, new intrinsics |
| 1.5 | 2020 | 1.5 | 6.5 | Wave intrinsics v2 |
| 1.6.2104 | 2021 | 1.6 | 6.6 | Enhanced Barriers, required for modern D3D12 |
| 1.7 | 2023 | 1.7 | 6.7 | Mesh shaders v2, work graphs |

**Real-World Failure Scenarios:**

1. **Windows 10 1809 (Oct 2018 Update)**:
   - Ships with DXC 1.0
   - Supports SM 6.0 only
   - Fails on SM 6.1+ features (wave intrinsics)

2. **Windows 10 21H1 (May 2021 Update)**:
   - Ships with DXC 1.5
   - Missing Enhanced Barriers (SM 6.6)
   - Fails on modern resource transition code

3. **Cryptic Error Messages**:
   ```
   DXC Error: invalid profile 'ps_6_5'
   → No indication that DXC version is too old

   HRESULT: 0x80070057 (E_INVALIDARG)
   → Generic error, no hint about version mismatch
   ```

---

## Official Documentation References

1. **IDxcVersionInfo Interface**:
   - [IDxcVersionInfo](https://learn.microsoft.com/windows/win32/api/dxcapi/nn-dxcapi-idxcversioninfo)
   - Quote: "Provides version information for the compiler."
   - Method: `GetVersion(UINT32* pMajor, UINT32* pMinor)`

2. **DXC Release Notes**:
   - [DirectXShaderCompiler Releases](https://github.com/microsoft/DirectXShaderCompiler/releases)
   - Documents breaking changes between versions
   - Minimum version requirements for SM 6.x features

3. **Shader Model 6.6 Requirements**:
   - [Shader Model 6.6](https://microsoft.github.io/DirectX-Specs/d3d/ShaderModel6_6.html)
   - Quote: "Requires DXC 1.6.2104 or later"
   - Enhanced Barriers require DXIL 1.6+

4. **Windows SDK DXC Versions**:
   - [Windows SDK Archive](https://developer.microsoft.com/windows/downloads/sdk-archive/)
   - Maps Windows versions to DXC versions
   - Critical for deployment compatibility

---

## Code Location Strategy

### Files to Modify:

1. **DXCCompiler.cpp** (`initialize` method):
   - Search for: `DxcCreateInstance(CLSID_DxcCompiler`
   - Context: After compiler creation, before setting initialized_
   - Action: Add version query and validation

2. **DXCCompiler.h** (class definition):
   - Search for: `class DXCCompiler`
   - Context: Public interface
   - Action: Add getter for DXC version info

3. **DXCCompiler.h** (member variables):
   - Search for: `bool initialized_`
   - Context: Private members
   - Action: Add version tracking members

4. **Device.cpp** (shader compilation):
   - Search for: `dxcCompiler.initialize()`
   - Context: DXC initialization call
   - Action: Check initialization result, log version

---

## Detection Strategy

### How to Reproduce Version Issues:

1. **Simulate Old DXC Version**:
   ```cpp
   // Test on Windows 10 1809 VM
   // Or replace dxcompiler.dll with old version from:
   // https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.4.2104

   auto result = dxcCompiler.initialize();
   EXPECT_TRUE(result.isOk());

   // Try to compile SM 6.5 shader
   result = dxcCompiler.compile(shader, "main", "ps_6_5", ...);
   // FAILS: "invalid profile 'ps_6_5'"
   ```

2. **No Version Logging**:
   ```bash
   # Current output (uninformative):
   DXCCompiler: Initialization successful (Shader Model 6.0+ enabled)

   # No indication of actual DXC version or capabilities
   ```

3. **Runtime Deployment Failure**:
   - Developer builds on Windows 11 (DXC 1.7) ✓
   - Customer runs on Windows 10 20H2 (DXC 1.5) ❌
   - Shader compilation fails with cryptic errors

### Verification After Fix:

1. Version logged at initialization:
   ```
   DXCCompiler: Detected DXC version 1.6.2104 (DXIL 1.6)
   DXCCompiler: Shader Model 6.6 support: AVAILABLE
   DXCCompiler: Enhanced Barriers support: AVAILABLE
   ```

2. Minimum version enforcement:
   ```
   DXCCompiler: ERROR - DXC version 1.3 detected
   DXCCompiler: Minimum required: 1.6.2104 (for SM 6.6 support)
   DXCCompiler: Please update Windows SDK or deploy dxcompiler.dll
   ```

3. Graceful degradation:
   ```
   DXCCompiler: WARNING - DXC 1.5 detected (SM 6.5 max)
   DXCCompiler: Enhanced Barriers unavailable, using legacy transitions
   ```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Version Query After Compiler Creation

**File:** `src/igl/d3d12/DXCCompiler.cpp`

**Locate:** `initialize()` method, after `DxcCreateInstance(CLSID_DxcCompiler)`

**Add:**
```cpp
Result DXCCompiler::initialize() {
  // ... existing DxcCreateInstance calls ...

  // Create DXC compiler
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create DxcCompiler: 0x%08X\n", hr);
    return Result(Result::Code::RuntimeError, "Failed to create DxcCompiler");
  }

  // ✅ NEW: Query DXC version information
  Microsoft::WRL::ComPtr<IDxcVersionInfo> versionInfo;
  hr = compiler_.As(&versionInfo);
  if (SUCCEEDED(hr)) {
    UINT32 major = 0, minor = 0;
    hr = versionInfo->GetVersion(&major, &minor);

    if (SUCCEEDED(hr)) {
      dxcVersionMajor_ = major;
      dxcVersionMinor_ = minor;

      IGL_LOG_INFO("DXCCompiler: Detected DXC version %u.%u\n", major, minor);

      // Determine supported DXIL version from DXC version
      // DXC 1.6.2104+ → DXIL 1.6 (SM 6.6)
      // DXC 1.5.x → DXIL 1.5 (SM 6.5)
      if (major > 1 || (major == 1 && minor >= 2104)) {
        maxSupportedDxilVersion_ = 0x10006;  // DXIL 1.6
        IGL_LOG_INFO("DXCCompiler: DXIL 1.6 support available (SM 6.6+)\n");
      } else if (major == 1 && minor >= 5) {
        maxSupportedDxilVersion_ = 0x10005;  // DXIL 1.5
        IGL_LOG_INFO("DXCCompiler: DXIL 1.5 support (SM 6.5 max)\n");
      } else {
        maxSupportedDxilVersion_ = 0x10000;  // DXIL 1.0
        IGL_LOG_INFO("DXCCompiler: Legacy DXC version (SM 6.0 only)\n");
      }
    } else {
      IGL_LOG_WARNING("DXCCompiler: Failed to query DXC version: 0x%08X\n", hr);
      // Continue with unknown version (non-fatal)
    }
  } else {
    IGL_LOG_WARNING("DXCCompiler: IDxcVersionInfo not available (old DXC?): 0x%08X\n", hr);
    // Very old DXC versions (pre-1.4) don't support IDxcVersionInfo
    // Assume minimum version
    dxcVersionMajor_ = 1;
    dxcVersionMinor_ = 0;
    maxSupportedDxilVersion_ = 0x10000;
  }

  // ... rest of initialization ...
}
```

---

#### Step 2: Enforce Minimum Version Requirement

**File:** `src/igl/d3d12/DXCCompiler.cpp`

**Add after version query:**

```cpp
// ✅ Enforce minimum DXC version for SM 6.6 support
constexpr UINT32 kMinDxcMajor = 1;
constexpr UINT32 kMinDxcMinor = 2104;  // DXC 1.6.2104 (April 2021)
constexpr const char* kMinDxcReason = "Shader Model 6.6 Enhanced Barriers support";

if (dxcVersionMajor_ < kMinDxcMajor ||
    (dxcVersionMajor_ == kMinDxcMajor && dxcVersionMinor_ < kMinDxcMinor)) {

  IGL_LOG_ERROR("DXCCompiler: Insufficient DXC version!\n");
  IGL_LOG_ERROR("  Detected: %u.%u\n", dxcVersionMajor_, dxcVersionMinor_);
  IGL_LOG_ERROR("  Required: %u.%u or later\n", kMinDxcMajor, kMinDxcMinor);
  IGL_LOG_ERROR("  Reason: %s\n", kMinDxcReason);
  IGL_LOG_ERROR("\n");
  IGL_LOG_ERROR("Solutions:\n");
  IGL_LOG_ERROR("  1. Update Windows SDK to 10.0.20348 or later\n");
  IGL_LOG_ERROR("  2. Deploy dxcompiler.dll + dxil.dll from DXC 1.6.2104+\n");
  IGL_LOG_ERROR("  3. Download from: https://github.com/microsoft/DirectXShaderCompiler/releases\n");

  return Result(Result::Code::RuntimeError,
                "DXC version too old - minimum 1.6.2104 required");
}

IGL_LOG_INFO("DXCCompiler: Version check passed ✓\n");
```

**Alternative (Graceful Degradation):**

If you want to support older DXC versions with reduced features:

```cpp
if (dxcVersionMajor_ < kMinDxcMajor ||
    (dxcVersionMajor_ == kMinDxcMajor && dxcVersionMinor_ < kMinDxcMinor)) {

  IGL_LOG_WARNING("DXCCompiler: Older DXC version detected (%u.%u)\n",
                  dxcVersionMajor_, dxcVersionMinor_);
  IGL_LOG_WARNING("  Some features will be unavailable:\n");
  IGL_LOG_WARNING("  - Shader Model 6.6+ (Enhanced Barriers)\n");
  IGL_LOG_WARNING("  - Wave Intrinsics 2.0\n");
  IGL_LOG_WARNING("  Recommended: Update to DXC 1.6.2104 or later\n");

  // Continue with reduced feature set
}
```

---

#### Step 3: Add Version Tracking Members

**File:** `src/igl/d3d12/DXCCompiler.h`

**Locate:** Private members section

**Add:**
```cpp
class DXCCompiler {
 public:
  DXCCompiler();
  ~DXCCompiler();

  Result initialize();
  bool isInitialized() const { return initialized_; }

  // ✅ NEW: Get DXC version information
  UINT32 getDxcVersionMajor() const { return dxcVersionMajor_; }
  UINT32 getDxcVersionMinor() const { return dxcVersionMinor_; }
  UINT32 getMaxSupportedDxilVersion() const { return maxSupportedDxilVersion_; }

  // ✅ NEW: Feature capability queries
  bool supportsShaderModel60() const { return maxSupportedDxilVersion_ >= 0x10000; }
  bool supportsShaderModel65() const { return maxSupportedDxilVersion_ >= 0x10005; }
  bool supportsShaderModel66() const { return maxSupportedDxilVersion_ >= 0x10006; }

  Result compile(/* ... */);

 private:
  Microsoft::WRL::ComPtr<IDxcUtils> utils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
  Microsoft::WRL::ComPtr<IDxcValidator> validator_;
  bool initialized_ = false;

  // ✅ NEW: DXC version tracking
  UINT32 dxcVersionMajor_ = 0;
  UINT32 dxcVersionMinor_ = 0;
  UINT32 maxSupportedDxilVersion_ = 0;  // Encoded as 0xMMMMNNNN (major.minor)
};
```

---

#### Step 4: Validate Shader Model Against DXC Capabilities

**File:** `src/igl/d3d12/DXCCompiler.cpp`

**Locate:** `compile()` method, before actual compilation

**Add:**
```cpp
Result DXCCompiler::compile(
    const char* source,
    size_t sourceLength,
    const char* entryPoint,
    const char* target,
    const char* debugName,
    uint32_t flags,
    std::vector<uint8_t>& outBytecode,
    std::string& outErrors) {

  if (!initialized_) {
    return Result(Result::Code::InvalidOperation, "DXC compiler not initialized");
  }

  // ✅ NEW: Validate shader target against DXC capabilities
  // Extract shader model from target (e.g., "ps_6_5" → 6.5)
  int major = 0, minor = 0;
  if (sscanf(target, "%*[a-z]_%d_%d", &major, &minor) == 2) {
    // Check if this shader model is supported by current DXC version
    if (major == 6) {
      if (minor >= 6 && !supportsShaderModel66()) {
        IGL_LOG_ERROR("DXCCompiler: Shader Model 6.6 requested but not supported\n");
        IGL_LOG_ERROR("  DXC version: %u.%u (max DXIL: 1.%u)\n",
                      dxcVersionMajor_, dxcVersionMinor_,
                      maxSupportedDxilVersion_ & 0xFFFF);
        IGL_LOG_ERROR("  Upgrade to DXC 1.6.2104 or later\n");

        outErrors = "Shader Model 6.6 not supported by current DXC version";
        return Result(Result::Code::RuntimeError, outErrors.c_str());
      }

      if (minor >= 5 && !supportsShaderModel65()) {
        IGL_LOG_ERROR("DXCCompiler: Shader Model 6.5 requested but not supported\n");
        outErrors = "Shader Model 6.5 not supported by current DXC version";
        return Result(Result::Code::RuntimeError, outErrors.c_str());
      }
    }
  }

  // Proceed with compilation...
  // ... existing compile code ...
}
```

---

#### Step 5: Add Comprehensive Logging at Initialization

**File:** `src/igl/d3d12/DXCCompiler.cpp`

**At end of `initialize()`:**

```cpp
Result DXCCompiler::initialize() {
  // ... all previous initialization code ...

  initialized_ = true;

  // ✅ Comprehensive initialization summary
  IGL_LOG_INFO("═══════════════════════════════════════════════════════\n");
  IGL_LOG_INFO("DXCCompiler: Initialization Complete\n");
  IGL_LOG_INFO("  DXC Version: %u.%u\n", dxcVersionMajor_, dxcVersionMinor_);
  IGL_LOG_INFO("  Max DXIL Version: 1.%u\n", maxSupportedDxilVersion_ & 0xFFFF);
  IGL_LOG_INFO("  Shader Model 6.0: %s\n", supportsShaderModel60() ? "✓" : "✗");
  IGL_LOG_INFO("  Shader Model 6.5: %s\n", supportsShaderModel65() ? "✓" : "✗");
  IGL_LOG_INFO("  Shader Model 6.6: %s\n", supportsShaderModel66() ? "✓" : "✗");
  IGL_LOG_INFO("  Validator: %s\n", validator_ ? "Available" : "Not Available");
  IGL_LOG_INFO("═══════════════════════════════════════════════════════\n");

  return Result();
}
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Shader compilation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*"
./build/Debug/IGLTests.exe --gtest_filter="*DXC*"
```

**New Tests Required:**

```cpp
TEST(DXCCompilerTest, VersionDetection) {
  DXCCompiler compiler;
  auto result = compiler.initialize();
  EXPECT_TRUE(result.isOk());

  // Version must be detected
  EXPECT_GT(compiler.getDxcVersionMajor(), 0);

  // Should support at least SM 6.0
  EXPECT_TRUE(compiler.supportsShaderModel60());

  IGL_LOG_INFO("DXC Version: %u.%u\n",
               compiler.getDxcVersionMajor(),
               compiler.getDxcVersionMinor());
}

TEST(DXCCompilerTest, MinimumVersionEnforcement) {
  DXCCompiler compiler;
  auto result = compiler.initialize();

  // Should either succeed with version 1.6.2104+ or fail gracefully
  if (result.isOk()) {
    EXPECT_GE(compiler.getDxcVersionMajor(), 1);
    if (compiler.getDxcVersionMajor() == 1) {
      EXPECT_GE(compiler.getDxcVersionMinor(), 2104);
    }
  } else {
    // Graceful failure expected on old DXC versions
    EXPECT_NE(result.message.find("DXC version"), std::string::npos);
  }
}

TEST(DXCCompilerTest, ShaderModelValidation) {
  DXCCompiler compiler;
  EXPECT_TRUE(compiler.initialize().isOk());

  // If DXC supports SM 6.6, test should pass
  if (compiler.supportsShaderModel66()) {
    std::vector<uint8_t> bytecode;
    std::string errors;

    const char* shader = "float4 main() : SV_Target { return 0; }";
    auto result = compiler.compile(shader, strlen(shader),
                                   "main", "ps_6_6",
                                   "test", 0,
                                   bytecode, errors);

    EXPECT_TRUE(result.isOk()) << "SM 6.6 should compile: " << errors;
  }
}
```

**Test Modifications Allowed:**
- ✅ Add DXC version detection tests
- ✅ Add shader model capability tests
- ❌ **DO NOT modify cross-platform test logic**

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should initialize DXC and log version
./test_all_sessions.bat
```

**Expected Output:**
```
═══════════════════════════════════════════════════════
DXCCompiler: Initialization Complete
  DXC Version: 1.7.2308
  Max DXIL Version: 1.7
  Shader Model 6.0: ✓
  Shader Model 6.5: ✓
  Shader Model 6.6: ✓
  Validator: Available
═══════════════════════════════════════════════════════
```

**Modifications Allowed:**
- ✅ None required
- ❌ **DO NOT modify session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass
- [ ] DXC version is queried and logged at initialization
- [ ] Minimum version requirement is enforced (DXC 1.6.2104+)
- [ ] Shader model validation prevents unsupported targets
- [ ] Graceful error messages guide users to solution
- [ ] Version information exposed via public API
- [ ] Render sessions initialize and log DXC version

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. DXC version is logged correctly
2. Minimum version enforcement works (test with old DXC if possible)
3. All tests pass with version checks enabled

**Post in chat:**
```
E-001 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- DXC version detection: WORKING (version X.Y detected)
- Minimum version enforcement: ENABLED (1.6.2104+)
- Shader model validation: ACTIVE

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix)

### Blocks:
- **E-002** (Shader model validation) - Needs DXC version info
- All shader compilation features requiring SM 6.6+

### Related:
- **H-009** (Shader model detection) - Device capabilities
- Enhanced Barriers implementation (requires SM 6.6)

---

## Implementation Priority

**Priority:** P0 - CRITICAL (Silent Runtime Failures)
**Estimated Effort:** 2-3 hours
**Risk:** Low (isolated change, DXC API is stable)
**Impact:** VERY HIGH - Prevents deployment failures and cryptic errors

---

## References

- [IDxcVersionInfo API](https://learn.microsoft.com/windows/win32/api/dxcapi/nn-dxcapi-idxcversioninfo)
- [DirectXShaderCompiler Releases](https://github.com/microsoft/DirectXShaderCompiler/releases)
- [Shader Model 6.6 Specification](https://microsoft.github.io/DirectX-Specs/d3d/ShaderModel6_6.html)
- [Windows SDK Archive](https://developer.microsoft.com/windows/downloads/sdk-archive/)
- [DXC Version to DXIL Mapping](https://github.com/microsoft/DirectXShaderCompiler/wiki/DXIL-Version-History)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
