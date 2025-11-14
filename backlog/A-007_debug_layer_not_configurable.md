# A-007: Debug Layer Not Configurable at Runtime

**Priority**: MEDIUM
**Category**: Architecture - Debugging
**Estimated Effort**: 2-3 days
**Status**: Not Started

---

## 1. Problem Statement

The D3D12 debug layer and validation settings are either hardcoded or only controllable at compile-time, making it difficult to enable/disable debugging features without rebuilding. Developers need the ability to configure debug layer settings at runtime through environment variables, configuration files, or API calls for efficient debugging and testing workflows.

**Symptoms**:
- Debug layer always on or always off, requiring rebuild to change
- Cannot enable GPU-based validation selectively
- Cannot adjust debug message filtering without code changes
- Cannot enable DRED (Device Removed Extended Data) on demand
- No way to configure DXGI debug settings at runtime

**Impact**:
- Slower development iteration (rebuild required for debug changes)
- Cannot collect verbose debug info from users without debug builds
- Difficult to reproduce specific validation scenarios
- Cannot performance test with minimal debug overhead
- Debugging production issues more difficult

---

## 2. Root Cause Analysis

The lack of runtime configurability stems from several issues:

### 2.1 Hardcoded Debug Layer Enablement
Debug layer is likely enabled only in preprocessor-controlled debug builds:

```cpp
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    debugController->EnableDebugLayer();
#endif
```

### 2.2 No Configuration System
There's no mechanism to read debug settings from:
- Environment variables (e.g., `IGL_D3D12_DEBUG=1`)
- Configuration files (e.g., `igl_config.json`)
- API parameters (e.g., `DeviceConfig.enableDebug`)

### 2.3 Missing Advanced Debug Features
Advanced debugging features may not be exposed:
- GPU-Based Validation (expensive but thorough)
- DRED (Device Removed Extended Data for crash analysis)
- Debug message severity filtering
- Break on specific error categories

### 2.4 No DXGI Debug Configuration
DXGI debug layer and info queue may not be configurable separately from D3D12 debug layer.

### 2.5 No Runtime Toggle
Once the device is created, there's no way to adjust debug settings without recreating the device.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Debug Layer Overview**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer
   - Comprehensive guide to D3D12 debug layer

2. **ID3D12Debug Interface**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12debug
   - Basic debug layer interface

3. **ID3D12Debug1 Interface**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12debug1
   - GPU-based validation support

4. **GPU-Based Validation**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation
   - How to enable and use GPU validation

5. **DRED (Device Removed Extended Data)**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/use-dred
   - Crash debugging with DRED

6. **Info Queue and Message Filtering**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12infoqueue
   - How to filter and handle debug messages

7. **DXGI Debug Interface**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-debug
   - DXGI-level debugging

**Sample Code**:

8. **D3D12 Debug Layer Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld
   - Various debug layer usage patterns

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for debug layer initialization**:
```
Pattern: "EnableDebugLayer" OR "ID3D12Debug" OR "D3D12GetDebugInterface"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/HWDevice.cpp
Focus: Where debug layer is enabled
```

**Search for debug preprocessor guards**:
```
Pattern: "#ifdef.*DEBUG" OR "#if defined.*DEBUG"
Files: src/igl/d3d12/**/*.cpp
Focus: Compile-time debug controls
```

**Search for validation settings**:
```
Pattern: "GPU.*Validation" OR "SetEnableGPUBasedValidation"
Files: src/igl/d3d12/**/*.cpp
Focus: Advanced validation features
```

**Search for info queue setup**:
```
Pattern: "ID3D12InfoQueue" OR "SetBreakOnSeverity"
Files: src/igl/d3d12/**/*.cpp
Focus: Message filtering and breaks
```

### 4.2 Likely File Locations

Based on typical D3D12 backend architecture:
- `src/igl/d3d12/Device.cpp` - Main device initialization with debug layer
- `src/igl/d3d12/HWDevice.cpp` - Hardware device setup
- `src/igl/d3d12/Common.cpp` or `src/igl/d3d12/Util.cpp` - Utility functions
- Platform-specific initialization files

### 4.3 Key Code Patterns to Find

Look for:
- `D3D12GetDebugInterface()` calls
- `EnableDebugLayer()` calls
- `#ifdef _DEBUG` or `#ifndef NDEBUG` guards
- Device creation with debug flags
- Info queue configuration

---

## 5. Detection Strategy

### 5.1 Add Instrumentation Code

**Step 1: Log debug layer status**

Add logging to show current debug configuration:

```cpp
void logDebugConfiguration() {
    IGL_LOG_INFO("=== D3D12 Debug Configuration ===\n");

#ifdef _DEBUG
    IGL_LOG_INFO("Build: DEBUG\n");
#else
    IGL_LOG_INFO("Build: RELEASE\n");
#endif

    IGL_LOG_INFO("Debug Layer: %s\n", isDebugLayerEnabled_ ? "ENABLED" : "DISABLED");
    IGL_LOG_INFO("GPU Validation: %s\n", isGPUValidationEnabled_ ? "ENABLED" : "DISABLED");
    IGL_LOG_INFO("DRED: %s\n", isDREDEnabled_ ? "ENABLED" : "DISABLED");
    IGL_LOG_INFO("DXGI Debug: %s\n", isDXGIDebugEnabled_ ? "ENABLED" : "DISABLED");
    IGL_LOG_INFO("=================================\n");
}
```

**Step 2: Check for environment variables**

```cpp
bool checkEnvironmentVariable(const char* varName) {
    const char* value = std::getenv(varName);
    bool enabled = value && (strcmp(value, "1") == 0 || _stricmp(value, "true") == 0);

    IGL_LOG_INFO("Environment variable %s: %s (enabled: %s)\n",
                 varName, value ? value : "(not set)", enabled ? "YES" : "NO");

    return enabled;
}
```

### 5.2 Manual Testing Procedure

**Test 1: Check current debug behavior**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

Look for:
- D3D12 debug messages in console
- Validation warnings/errors
- Whether debug layer is active

**Test 2: Try environment variable (will fail initially)**
```bash
set IGL_D3D12_DEBUG=1
./build/Release/IGLTests.exe --gtest_filter="*D3D12*"
```

Currently will not enable debug layer in release builds.

**Test 3: Check for GPU validation**

GPU validation has significant performance impact. Currently may be always on or always off.

### 5.3 Expected Baseline Metrics

**Current Behavior**:
- Debug layer: ON in debug builds, OFF in release builds
- GPU validation: Likely OFF always
- DRED: Unknown, likely OFF
- No runtime configurability
- Requires rebuild to change settings

**Target Behavior**:
- Configurable via environment variables
- Configurable via API
- Works in both debug and release builds
- Advanced features available on-demand
- No rebuild required

---

## 6. Fix Guidance

### 6.1 Define Debug Configuration Structure

Create a comprehensive debug configuration:

```cpp
// DebugConfig.h
struct D3D12DebugConfig {
    // Basic debug layer
    bool enableDebugLayer = false;

    // Advanced validation (expensive)
    bool enableGPUBasedValidation = false;

    // DRED for crash analysis
    bool enableDRED = false;
    bool enableAutoName = true;  // Auto-name resources for DRED

    // DXGI debug
    bool enableDXGIDebug = false;

    // Info queue settings
    bool breakOnError = true;
    bool breakOnWarning = false;
    bool breakOnCorruption = true;

    // Message filtering
    bool suppressInfoMessages = true;
    bool logAllMessages = false;

    // Load from environment variables
    static D3D12DebugConfig fromEnvironment() {
        D3D12DebugConfig config;

        // Read environment variables
        config.enableDebugLayer = getEnvBool("IGL_D3D12_DEBUG", false);
        config.enableGPUBasedValidation = getEnvBool("IGL_D3D12_GPU_VALIDATION", false);
        config.enableDRED = getEnvBool("IGL_D3D12_DRED", false);
        config.enableDXGIDebug = getEnvBool("IGL_DXGI_DEBUG", false);
        config.breakOnError = getEnvBool("IGL_D3D12_BREAK_ON_ERROR", true);
        config.breakOnWarning = getEnvBool("IGL_D3D12_BREAK_ON_WARNING", false);

        return config;
    }

private:
    static bool getEnvBool(const char* name, bool defaultValue) {
        const char* value = std::getenv(name);
        if (!value) return defaultValue;
        return (strcmp(value, "1") == 0) || (_stricmp(value, "true") == 0);
    }
};
```

### 6.2 Implement Configurable Debug Layer Initialization

Replace hardcoded initialization:

```cpp
// In Device initialization
Result Device::initializeDebugLayer(const D3D12DebugConfig& config) {
    debugConfig_ = config;

    // Apply defaults based on build type if not explicitly set
#ifdef _DEBUG
    if (!debugConfig_.enableDebugLayer) {
        debugConfig_.enableDebugLayer = true;  // Default ON for debug builds
        IGL_LOG_INFO("Debug build: Enabling debug layer by default\n");
    }
#endif

    if (!debugConfig_.enableDebugLayer) {
        IGL_LOG_INFO("Debug layer disabled\n");
        return Result();
    }

    // Enable basic debug layer
    ComPtr<ID3D12Debug> debugController;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (FAILED(hr)) {
        IGL_LOG_WARNING("Failed to get D3D12 debug interface (SDK not installed?)\n");
        return Result();
    }

    debugController->EnableDebugLayer();
    IGL_LOG_INFO("D3D12 debug layer enabled\n");

    // Enable GPU-based validation if requested
    if (debugConfig_.enableGPUBasedValidation) {
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1))) {
            debugController1->SetEnableGPUBasedValidation(TRUE);
            IGL_LOG_INFO("GPU-based validation enabled (performance impact!)\n");
        } else {
            IGL_LOG_WARNING("GPU-based validation not available\n");
        }
    }

    // Enable DRED
    if (debugConfig_.enableDRED) {
        enableDRED();
    }

    return Result();
}
```

### 6.3 Implement DRED Support

Add DRED for crash debugging:

```cpp
void Device::enableDRED() {
    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));

    if (SUCCEEDED(hr)) {
        // Turn on auto-breadcrumbs and page fault reporting
        dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

        IGL_LOG_INFO("DRED enabled (auto-breadcrumbs and page fault reporting)\n");
    } else {
        IGL_LOG_WARNING("DRED not available (requires Windows 10 May 2019 Update or later)\n");
    }
}

// On device removal, retrieve DRED data
void Device::handleDeviceRemoved(HRESULT reason) {
    IGL_LOG_ERROR("Device removed: HRESULT = 0x%08X\n", reason);

    if (debugConfig_.enableDRED) {
        ComPtr<ID3D12DeviceRemovedExtendedData> dred;
        if (SUCCEEDED(device_.As(&dred))) {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT dredAutoBreadcrumbsOutput;
            D3D12_DRED_PAGE_FAULT_OUTPUT dredPageFaultOutput;

            if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&dredAutoBreadcrumbsOutput))) {
                IGL_LOG_ERROR("DRED Auto-Breadcrumbs:\n");
                // Log breadcrumb trail showing where GPU hung
                // ... (implementation details)
            }

            if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&dredPageFaultOutput))) {
                IGL_LOG_ERROR("DRED Page Fault:\n");
                // Log page fault information
                // ... (implementation details)
            }
        }
    }
}
```

### 6.4 Configure Info Queue

Set up message filtering and break behavior:

```cpp
void Device::configureInfoQueue() {
    if (!debugConfig_.enableDebugLayer) {
        return;
    }

    ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(device_.As(&infoQueue))) {
        return;
    }

    // Set break on severity
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, debugConfig_.breakOnCorruption);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, debugConfig_.breakOnError);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, debugConfig_.breakOnWarning);

    IGL_LOG_INFO("Info queue breaks: Corruption=%s, Error=%s, Warning=%s\n",
                 debugConfig_.breakOnCorruption ? "ON" : "OFF",
                 debugConfig_.breakOnError ? "ON" : "OFF",
                 debugConfig_.breakOnWarning ? "ON" : "OFF");

    // Filter messages
    if (debugConfig_.suppressInfoMessages) {
        D3D12_MESSAGE_SEVERITY suppressedSeverities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumSeverities = ARRAYSIZE(suppressedSeverities);
        filter.DenyList.pSeverityList = suppressedSeverities;

        infoQueue->PushStorageFilter(&filter);
        IGL_LOG_INFO("Suppressed INFO severity messages\n");
    }

    // Suppress specific known benign warnings (optional)
    D3D12_MESSAGE_ID suppressedMessages[] = {
        // Add message IDs to suppress, e.g.:
        // D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
    };

    if (ARRAYSIZE(suppressedMessages) > 0) {
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = ARRAYSIZE(suppressedMessages);
        filter.DenyList.pIDList = suppressedMessages;

        infoQueue->PushStorageFilter(&filter);
        IGL_LOG_INFO("Suppressed %zu specific message IDs\n", ARRAYSIZE(suppressedMessages));
    }
}
```

### 6.5 Enable DXGI Debug

Add DXGI-level debugging:

```cpp
void Device::enableDXGIDebug() {
    if (!debugConfig_.enableDXGIDebug) {
        return;
    }

    typedef HRESULT(WINAPI* DXGIGetDebugInterfaceProc)(REFIID, void**);

    HMODULE dxgiDebug = LoadLibraryA("dxgidebug.dll");
    if (!dxgiDebug) {
        IGL_LOG_WARNING("Failed to load dxgidebug.dll\n");
        return;
    }

    auto DXGIGetDebugInterface1 = reinterpret_cast<DXGIGetDebugInterfaceProc>(
        GetProcAddress(dxgiDebug, "DXGIGetDebugInterface1"));

    if (DXGIGetDebugInterface1) {
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)))) {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            IGL_LOG_INFO("DXGI debug enabled\n");
        }
    }
}
```

### 6.6 Update Device Creation Flow

Integrate debug configuration:

```cpp
// In Device::create() or initialize()
Result Device::create(const DeviceConfig& config) {
    // Load debug config from environment, then override with config
    D3D12DebugConfig debugConfig = D3D12DebugConfig::fromEnvironment();

    // Allow API config to override environment
    if (config.debugConfig) {
        debugConfig = *config.debugConfig;
    }

    // Initialize debug layer BEFORE device creation
    Result result = initializeDebugLayer(debugConfig);
    if (!result.isOk()) {
        return result;
    }

    // Create DXGI factory
    UINT dxgiFactoryFlags = 0;
    if (debugConfig.enableDebugLayer || debugConfig.enableDXGIDebug) {
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory_));
    if (FAILED(hr)) {
        return getResultFromHRESULT(hr);
    }

    // Select adapter and create device
    // ...

    // Configure info queue AFTER device creation
    configureInfoQueue();

    // Enable DXGI debug AFTER factory creation
    enableDXGIDebug();

    // Log final configuration
    logDebugConfiguration();

    return Result();
}
```

---

## 7. Testing Requirements

### 7.1 Functional Testing

**Test 1: Default behavior (debug builds)**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: Debug layer enabled automatically, messages visible.

**Test 2: Default behavior (release builds)**
```bash
./build/Release/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: Debug layer disabled, no validation overhead.

**Test 3: Enable debug in release build**
```bash
set IGL_D3D12_DEBUG=1
./build/Release/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: Debug layer enabled, validation messages appear.

**Test 4: Enable GPU validation**
```bash
set IGL_D3D12_DEBUG=1
set IGL_D3D12_GPU_VALIDATION=1
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*"
```

**Expected**: Slower execution, thorough GPU-side validation.

**Test 5: Enable DRED**
```bash
set IGL_D3D12_DRED=1
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Expected**: DRED enabled message, breadcrumb tracking active.

### 7.2 Configuration Testing

**Test all environment variables**:
```bash
set IGL_D3D12_DEBUG=1
set IGL_D3D12_GPU_VALIDATION=1
set IGL_D3D12_DRED=1
set IGL_DXGI_DEBUG=1
set IGL_D3D12_BREAK_ON_ERROR=0
set IGL_D3D12_BREAK_ON_WARNING=1
./build/Debug/samples/Textured3DCube
```

Verify each setting takes effect.

**Test API configuration override**:
```cpp
DeviceConfig config;
config.debugConfig = std::make_unique<D3D12DebugConfig>();
config.debugConfig->enableDebugLayer = true;
config.debugConfig->enableGPUBasedValidation = false;

auto device = Device::create(config);
```

### 7.3 Performance Testing

**Measure overhead of different debug levels**:

| Configuration | Frame Time |
|--------------|------------|
| No debug | Baseline |
| Debug layer | +5-10% |
| Debug + GPU validation | +200-500% |
| Debug + DRED | +10-15% |

### 7.4 Test Modification Restrictions

**CRITICAL CONSTRAINTS**:
- Do NOT modify any existing test assertions
- Do NOT change test expected values
- Do NOT alter test logic or flow
- Debug configuration is transparent to tests
- All tests must pass regardless of debug settings

---

## 8. Definition of Done

### 8.1 Implementation Checklist

- [ ] D3D12DebugConfig structure defined
- [ ] Environment variable reading implemented
- [ ] API configuration parameter added
- [ ] Debug layer configurable at runtime
- [ ] GPU-based validation configurable
- [ ] DRED support implemented
- [ ] DXGI debug configurable
- [ ] Info queue filtering implemented
- [ ] Break-on-error configurable
- [ ] Comprehensive debug logging added

### 8.2 Testing Checklist

- [ ] All unit tests pass with debug layer OFF
- [ ] All unit tests pass with debug layer ON
- [ ] Environment variables control debug settings
- [ ] API configuration overrides work
- [ ] GPU validation can be enabled/disabled
- [ ] DRED messages appear when enabled
- [ ] No crashes with any debug configuration

### 8.3 Documentation Checklist

- [ ] Environment variables documented
- [ ] API configuration documented
- [ ] Debug feature descriptions documented
- [ ] Performance impact of each option documented
- [ ] Troubleshooting guide created

### 8.4 Sign-Off Criteria

**Before proceeding with this task, YOU MUST**:
1. Read and understand all 11 sections of this document
2. Understand D3D12 debug layer features
3. Verify you can build both debug and release configurations
4. Confirm you can build and run the test suite
5. Get explicit approval to proceed with implementation

**Do not make any code changes until all criteria are met and approval is given.**

---

## 9. Related Issues

### 9.1 Blocking Issues
None - this can be implemented independently.

### 9.2 Blocked Issues
- Better debugging will help diagnose other issues
- Required for production debugging scenarios

### 9.3 Related Issues
- Complements all bug fix tasks
- Useful for performance profiling (disable debug for accurate metrics)
- Essential for device removal debugging

---

## 10. Implementation Priority

**Priority Level**: MEDIUM

**Rationale**:
- Significantly improves development workflow
- Essential for production debugging
- Low implementation complexity
- No risk to release builds (opt-in)
- Standard D3D12 best practice

**Recommended Order**:
1. Define configuration structure and environment variable reading (Day 1)
2. Implement configurable debug layer (Day 1)
3. Add GPU validation and DRED support (Day 2)
4. Add info queue configuration (Day 2)
5. Testing and documentation (Day 3)

**Estimated Timeline**:
- Day 1: Configuration infrastructure and basic debug layer
- Day 2: Advanced features (GPU validation, DRED, info queue)
- Day 3: Testing, validation, documentation

---

## 11. References

### 11.1 Microsoft Official Documentation
- Debug Layer: https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer
- ID3D12Debug: https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12debug
- ID3D12Debug1: https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12debug1
- GPU-Based Validation: https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation
- DRED: https://docs.microsoft.com/en-us/windows/win32/direct3d12/use-dred
- ID3D12InfoQueue: https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12infoqueue
- DXGI Debug: https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-debug

### 11.2 Sample Code References
- D3D12HelloWorld: https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld
  - Various debug layer patterns

### 11.3 Additional Reading
- PIX for Windows: https://devblogs.microsoft.com/pix/
- Debugging D3D12 Applications: https://devblogs.microsoft.com/directx/dred/
- GPU Validation Best Practices: https://devblogs.microsoft.com/directx/using-d3d12-gpu-based-validation/

### 11.4 Internal Codebase
- Search for "EnableDebugLayer" in src/igl/d3d12/
- Review current debug initialization in Device.cpp
- Check for existing configuration infrastructure

---

**Document Version**: 1.0
**Last Updated**: 2025-11-12
**Author**: Development Team
**Reviewer**: [Pending]
