# A-010: HDR Output Capabilities Not Detected

## 1. Problem Statement

**Severity:** MEDIUM
**Category:** A - Architecture
**Status:** Open
**Related Issues:** A-009

### Detailed Explanation

The D3D12 backend does not detect or expose HDR (High Dynamic Range) output capabilities, preventing applications from utilizing wide color gamut and HDR displays. The current implementation has no infrastructure for:

1. **HDR Display Detection:** No queries to `IDXGIOutput6::GetDesc1()` to check for HDR10 support
2. **Color Space Enumeration:** No calls to `IDXGISwapChain3::CheckColorSpaceSupport()` to verify supported color spaces
3. **Color Space Configuration:** No mechanism to set swapchain color space via `SetColorSpace1()`
4. **Metadata Configuration:** No support for HDR metadata via `SetHDRMetaData()`
5. **Format Validation:** No verification that swapchain format supports HDR (e.g., DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT)

### The Danger

- **Missed HDR Opportunities:** Applications running on HDR displays render in SDR (Standard Dynamic Range), wasting display capabilities
- **Incorrect Color Rendering:** Content may appear washed out or over-saturated when HDR is available but not configured
- **No Application Control:** Users cannot enable HDR rendering even if their hardware supports it
- **Competitive Disadvantage:** Modern graphics applications require HDR support for visual parity with competitors
- **Future-Proofing:** HDR adoption is increasing; lack of support limits IGL's applicability

## 2. Root Cause Analysis

### Current Implementation

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.h`:

```cpp
// Lines 78-98: No HDR-related members or methods
class D3D12Context {
public:
  // ... existing methods ...

  // NO HDR DETECTION:
  // - No getHDRSupport() method
  // - No getSupportedColorSpaces() query
  // - No setColorSpace() configuration
  // - No HDR metadata support
```

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`:

```cpp
void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.Width = width;
  swapChainDesc.Height = height;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // SDR FORMAT ONLY
  // ... other settings ...

  // NO HDR DETECTION:
  // - No query to IDXGIOutput6::GetDesc1() for HDR support
  // - No CheckColorSpaceSupport() call
  // - No color space configuration
}
```

### Technical Explanation

The root causes are:

1. **Fixed SDR Format:** Swapchain always uses `DXGI_FORMAT_R8G8B8A8_UNORM` (8-bit sRGB), which cannot represent HDR values
2. **No Output Query:** Code never calls `IDXGIOutput::GetDesc1()` to check `DXGI_OUTPUT_DESC1::ColorSpace`
3. **No Color Space Support Check:** `IDXGISwapChain3::CheckColorSpaceSupport()` is never called to enumerate available color spaces
4. **Missing IDXGIOutput6:** Code doesn't query for `IDXGIOutput6` interface required for HDR detection
5. **No Capability Exposure:** Even if HDR is detected, there's no IGL API to expose this to applications

## 3. Official Documentation References

1. **Using DirectX with High Dynamic Range Displays**
   https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
   *Key Guidance:* Query output for HDR support, use 10-bit or 16-bit float formats, set appropriate color space

2. **IDXGIOutput6::GetDesc1**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgioutput6-getdesc1
   *Key Guidance:* Returns `DXGI_OUTPUT_DESC1` with `ColorSpace` field indicating HDR support

3. **IDXGISwapChain3::CheckColorSpaceSupport**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-checkcolorspacesupport
   *Key Guidance:* Query which color spaces are supported by the current swapchain configuration

4. **IDXGISwapChain3::SetColorSpace1**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-setcolorspace1
   *Key Guidance:* Configure swapchain color space for HDR rendering

5. **DXGI_COLOR_SPACE_TYPE Enumeration**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgicommon/ne-dxgicommon-dxgi_color_space_type
   *Key Guidance:* Available color spaces include RGB_FULL_G2084_NONE_P2020 (HDR10) and RGB_FULL_G10_NONE_P709 (scRGB)

## 4. Code Location Strategy

### Files to Modify

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.h`
- **Search Pattern:** `bool isTearingSupported() const`
- **Context:** Capability query methods around line 116
- **Action:** Add HDR detection methods and color space tracking

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`
- **Search Pattern:** `createSwapChain` and `IDXGISwapChain3`
- **Context:** Swapchain creation around line 560
- **Action:** Add HDR output detection and color space enumeration

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`
- **Search Pattern:** `DXGI_FORMAT_R8G8B8A8_UNORM`
- **Context:** Swapchain format configuration
- **Action:** Allow HDR formats based on display capabilities

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\PlatformDevice.h` (if exists)
- **Search Pattern:** `class PlatformDevice`
- **Context:** Platform-specific device capabilities
- **Action:** Expose HDR capabilities through IGL's platform device interface

## 5. Detection Strategy

### How to Reproduce

**Test Scenario 1: HDR Display Detection**

```cpp
// On system with HDR-capable display
auto device = createD3D12Device();

// Current behavior: No HDR information available
// Expected: Should detect HDR display and report supported color spaces

// Try to query HDR support (not implemented):
// bool hdrSupported = device->supportsHDR();  // Method doesn't exist
```

**Test Scenario 2: Manual HDR Verification**

```cpp
// Query adapter output manually
Microsoft::WRL::ComPtr<IDXGIOutput> output;
swapChain->GetContainingOutput(&output);

Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
output.As(&output6);

if (output6) {
  DXGI_OUTPUT_DESC1 desc{};
  output6->GetDesc1(&desc);

  // desc.ColorSpace will be DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 for HDR10
  // Current IGL implementation never performs this query
}
```

**Manual Testing:**

1. Run IGL application on HDR-capable Windows 10/11 system
2. Enable HDR in Windows Settings > Display > HDR
3. Observe that application renders in SDR (8-bit color)
4. Check logs - no mention of HDR capabilities

### Verification Steps

After implementing HDR detection:

```bash
# Build with HDR detection enabled
cd C:\Users\rudyb\source\repos\igl\igl\build
cmake --build . --config Debug

# Run on HDR-capable system
cd C:\Users\rudyb\source\repos\igl\igl\build\shell\renderSessions\Debug
.\igl_shell_RenderSessions.exe --session=HelloWorld

# Expected log output:
# "D3D12Context: Querying HDR capabilities..."
# "D3D12Context: Output color space: DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 (HDR10)"
# "D3D12Context: Supported color spaces:"
# "  - DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 (SDR)"
# "  - DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 (HDR10)"
# "D3D12Context: HDR output available: YES"
```

## 6. Fix Guidance

### Implementation Steps

**Step 1: Add HDR State Tracking to D3D12Context.h**

Add after line 218 (after tearing support variables):

```cpp
// A-010: HDR output detection and configuration
bool hdrDisplayDetected_ = false;            // Does connected output support HDR?
DXGI_COLOR_SPACE_TYPE currentColorSpace_ = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;  // Active color space
std::vector<DXGI_COLOR_SPACE_TYPE> supportedColorSpaces_;  // All supported color spaces

// HDR capability queries
bool isHDRSupported() const { return hdrDisplayDetected_; }
DXGI_COLOR_SPACE_TYPE getCurrentColorSpace() const { return currentColorSpace_; }
const std::vector<DXGI_COLOR_SPACE_TYPE>& getSupportedColorSpaces() const {
  return supportedColorSpaces_;
}

// Detect HDR capabilities of connected display
void detectHDRCapabilities();

// Configure swapchain for HDR rendering (if supported)
Result setHDRColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace);
```

**Step 2: Implement detectHDRCapabilities() in D3D12Context.cpp**

Add new function before `createSwapChain()`:

```cpp
void D3D12Context::detectHDRCapabilities() {
  if (!swapChain_) {
    IGL_LOG_ERROR("D3D12Context::detectHDRCapabilities() - No swapchain\n");
    hdrDisplayDetected_ = false;
    return;
  }

  // Get the output containing the swapchain
  Microsoft::WRL::ComPtr<IDXGIOutput> output;
  HRESULT hr = swapChain_->GetContainingOutput(output.GetAddressOf());
  if (FAILED(hr)) {
    IGL_LOG_INFO("D3D12Context: GetContainingOutput failed (0x%08X), assuming SDR\n", hr);
    hdrDisplayDetected_ = false;
    return;
  }

  // Query IDXGIOutput6 for HDR information
  Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
  hr = output.As(&output6);
  if (FAILED(hr)) {
    IGL_LOG_INFO("D3D12Context: IDXGIOutput6 not available, HDR not supported\n");
    hdrDisplayDetected_ = false;
    return;
  }

  // Get output description with color space information
  DXGI_OUTPUT_DESC1 outputDesc{};
  hr = output6->GetDesc1(&outputDesc);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: GetDesc1 failed (0x%08X)\n", hr);
    hdrDisplayDetected_ = false;
    return;
  }

  // Log output capabilities
  IGL_LOG_INFO("D3D12Context: Display output capabilities:\n");
  IGL_LOG_INFO("  Device name: %ls\n", outputDesc.DeviceName);
  IGL_LOG_INFO("  Color space: %u\n", outputDesc.ColorSpace);
  IGL_LOG_INFO("  Red primary: (%.3f, %.3f)\n", outputDesc.RedPrimary[0], outputDesc.RedPrimary[1]);
  IGL_LOG_INFO("  Green primary: (%.3f, %.3f)\n", outputDesc.GreenPrimary[0], outputDesc.GreenPrimary[1]);
  IGL_LOG_INFO("  Blue primary: (%.3f, %.3f)\n", outputDesc.BluePrimary[0], outputDesc.BluePrimary[1]);
  IGL_LOG_INFO("  White point: (%.3f, %.3f)\n", outputDesc.WhitePoint[0], outputDesc.WhitePoint[1]);
  IGL_LOG_INFO("  Min luminance: %.3f nits\n", outputDesc.MinLuminance);
  IGL_LOG_INFO("  Max luminance: %.3f nits\n", outputDesc.MaxLuminance);
  IGL_LOG_INFO("  Max full-frame luminance: %.3f nits\n", outputDesc.MaxFullFrameLuminance);

  // Detect HDR support based on color space
  // HDR10: DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 (value 12)
  // scRGB: DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 (value 9)
  hdrDisplayDetected_ = (outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) ||
                        (outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);

  IGL_LOG_INFO("D3D12Context: HDR display detected: %s\n", hdrDisplayDetected_ ? "YES" : "NO");

  // Enumerate supported color spaces for current swapchain format
  supportedColorSpaces_.clear();

  // Check common color spaces
  const DXGI_COLOR_SPACE_TYPE colorSpacesToCheck[] = {
    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,        // SDR (sRGB)
    DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,        // scRGB (linear, HDR)
    DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,     // HDR10 (ST.2084 PQ, Rec.2020)
    DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,      // SDR (limited range)
    DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020,   // HDR10 (limited range)
  };

  IGL_LOG_INFO("D3D12Context: Supported color spaces for current swapchain:\n");

  for (DXGI_COLOR_SPACE_TYPE colorSpace : colorSpacesToCheck) {
    UINT colorSpaceSupport = 0;
    hr = swapChain_->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport);

    if (SUCCEEDED(hr) && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
      supportedColorSpaces_.push_back(colorSpace);

      const char* colorSpaceName = "Unknown";
      switch (colorSpace) {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
          colorSpaceName = "SDR (sRGB, Rec.709)";
          break;
        case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
          colorSpaceName = "scRGB (Linear HDR, Rec.709)";
          break;
        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
          colorSpaceName = "HDR10 (ST.2084 PQ, Rec.2020)";
          break;
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
          colorSpaceName = "SDR Limited Range (Rec.709)";
          break;
        case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
          colorSpaceName = "HDR10 Limited Range (Rec.2020)";
          break;
      }

      IGL_LOG_INFO("  ✓ %s (colorspace=%u)\n", colorSpaceName, colorSpace);
    }
  }

  if (supportedColorSpaces_.empty()) {
    IGL_LOG_ERROR("D3D12Context: No supported color spaces found!\n");
  }

  // Get current color space
  currentColorSpace_ = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;  // Default to SDR
}
```

**Step 3: Implement setHDRColorSpace() for Configuration**

Add after `detectHDRCapabilities()`:

```cpp
Result D3D12Context::setHDRColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace) {
  if (!swapChain_) {
    return Result(Result::Code::RuntimeError, "No swapchain available");
  }

  // Check if color space is supported
  bool isSupported = std::find(supportedColorSpaces_.begin(),
                               supportedColorSpaces_.end(),
                               colorSpace) != supportedColorSpaces_.end();

  if (!isSupported) {
    IGL_LOG_ERROR("D3D12Context: Color space %u not supported by swapchain\n", colorSpace);
    return Result(Result::Code::Unsupported, "Color space not supported");
  }

  // Set color space
  HRESULT hr = swapChain_->SetColorSpace1(colorSpace);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: SetColorSpace1 failed: 0x%08X\n", hr);
    return Result(Result::Code::RuntimeError, "Failed to set color space");
  }

  currentColorSpace_ = colorSpace;

  const char* colorSpaceName = "Unknown";
  switch (colorSpace) {
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
      colorSpaceName = "SDR (sRGB)";
      break;
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
      colorSpaceName = "scRGB (Linear HDR)";
      break;
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
      colorSpaceName = "HDR10 (ST.2084 PQ)";
      break;
  }

  IGL_LOG_INFO("D3D12Context: Color space set to %s\n", colorSpaceName);

  return Result();
}
```

**Step 4: Call HDR Detection in createSwapChain()**

Add after swapchain creation (around line 590):

```cpp
void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  // ... existing swapchain creation code ...

  hr = tempSwapChain.As(&swapChain_);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get IDXGISwapChain3 interface");
  }

  // A-010: Detect HDR capabilities after swapchain creation
  detectHDRCapabilities();

  // ... rest of initialization ...
}
```

**Step 5: Re-Detect HDR After Resize**

Add in `recreateSwapChain()`:

```cpp
Result D3D12Context::recreateSwapChain(uint32_t width, uint32_t height) {
  // ... existing resize logic ...

  HRESULT hr = swapChain_->ResizeBuffers(...);
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to resize swapchain");
  }

  // A-010: Re-detect HDR after resize (output may have changed)
  detectHDRCapabilities();

  // ... rest of resize logic ...
  return Result();
}
```

**Step 6: Add Optional HDR Format Support**

For applications that want HDR rendering, add format selection logic:

```cpp
// In createSwapChain(), allow HDR format selection
DXGI_FORMAT swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;  // Default SDR

// Optional: Check environment variable for HDR format request
char hdrEnv[16] = {};
if (GetEnvironmentVariableA("IGL_D3D12_HDR", hdrEnv, sizeof(hdrEnv)) > 0) {
  if (strcmp(hdrEnv, "HDR10") == 0) {
    swapchainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;  // 10-bit for HDR10
    IGL_LOG_INFO("D3D12Context: Requesting HDR10 format (R10G10B10A2)\n");
  } else if (strcmp(hdrEnv, "SCRGB") == 0) {
    swapchainFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;  // 16-bit float for scRGB
    IGL_LOG_INFO("D3D12Context: Requesting scRGB format (R16G16B16A16_FLOAT)\n");
  }
}

swapChainDesc.Format = swapchainFormat;
```

**Rationale:**
- `IDXGIOutput6::GetDesc1()` provides ground truth for HDR display capabilities
- `CheckColorSpaceSupport()` verifies which color spaces work with current swapchain format
- Detection is automatic but HDR activation requires explicit format and color space configuration
- Color space can be changed at runtime without recreating swapchain
- Logging provides clear diagnostic information for HDR troubleshooting

### Before/After Code Comparison

**Before (no HDR detection):**
```cpp
void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // Always SDR
  // ... create swapchain ...
  // NO HDR DETECTION
}

// No way to query HDR support
// bool hdrSupported = ???;  // Method doesn't exist
```

**After (comprehensive HDR detection):**
```cpp
void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // Default, can be changed for HDR
  // ... create swapchain ...

  // Detect HDR capabilities
  detectHDRCapabilities();

  // Later, application can enable HDR if supported
  if (isHDRSupported()) {
    setHDRColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
  }
}
```

## 7. Testing Requirements

### Unit Tests

**Command:**
```bash
cd C:\Users\rudyb\source\repos\igl\igl\build
ctest -C Debug -R "d3d12.*hdr" --verbose
```

**Allowed Test Modifications:**
- Add new test `HDRCapabilityDetection` to verify HDR detection on capable systems
- Add test `ColorSpaceEnumeration` to verify supported color spaces are queried
- Add test `HDRColorSpaceConfiguration` to verify SetColorSpace1() is called correctly

**New Test Cases to Add:**

```cpp
// In D3D12ContextTest.cpp
TEST(D3D12HDRTest, CapabilityDetection) {
  auto ctx = createWindowedContext();

  // Verify HDR detection ran
  bool hdrSupported = ctx.isHDRSupported();

  // Log result (will be false on SDR systems, true on HDR systems)
  std::cout << "HDR supported: " << (hdrSupported ? "YES" : "NO") << "\n";

  // Verify color space list was populated
  auto colorSpaces = ctx.getSupportedColorSpaces();
  EXPECT_GT(colorSpaces.size(), 0) << "No supported color spaces detected";

  // SDR color space should always be supported
  bool sdrSupported = std::find(colorSpaces.begin(), colorSpaces.end(),
                                DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) != colorSpaces.end();
  EXPECT_TRUE(sdrSupported) << "SDR color space not supported";
}

TEST(D3D12HDRTest, ColorSpaceConfiguration) {
  auto ctx = createWindowedContext();

  if (!ctx.isHDRSupported()) {
    GTEST_SKIP() << "HDR not supported on this system";
  }

  // Try to set HDR10 color space
  auto result = ctx.setHDRColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);

  if (result.isOk()) {
    EXPECT_EQ(ctx.getCurrentColorSpace(), DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
  }
}
```

### Render Session Tests

**Commands:**
```bash
cd C:\Users\rudyb\source\repos\igl\igl\build\shell\renderSessions\Debug

# Test HDR detection (run on HDR-capable system with Windows HDR enabled)
.\igl_shell_RenderSessions.exe --session=HelloWorld --frames=100

# Check log for HDR detection output:
# Expected on HDR display:
# "D3D12Context: HDR display detected: YES"
# "D3D12Context: Supported color spaces:"
# "  ✓ SDR (sRGB, Rec.709)"
# "  ✓ HDR10 (ST.2084 PQ, Rec.2020)"

# Expected on SDR display:
# "D3D12Context: HDR display detected: NO"
# "D3D12Context: Supported color spaces:"
# "  ✓ SDR (sRGB, Rec.709)"
```

## 8. Definition of Done

### Completion Criteria

- [ ] `detectHDRCapabilities()` implemented with IDXGIOutput6::GetDesc1() query
- [ ] `hdrDisplayDetected_` flag set based on output color space
- [ ] `supportedColorSpaces_` populated via CheckColorSpaceSupport() for common color spaces
- [ ] `isHDRSupported()` method exposed for application queries
- [ ] `setHDRColorSpace()` implemented to configure swapchain color space
- [ ] HDR detection called automatically after swapchain creation and resize
- [ ] Comprehensive logging of display capabilities (luminance range, primaries, color space)
- [ ] Color space support logged with descriptive names
- [ ] Environment variable `IGL_D3D12_HDR` allows testing HDR formats (optional)
- [ ] Unit tests verify HDR detection on capable systems
- [ ] Render sessions log HDR capabilities without errors

### User Confirmation Required

Before proceeding to next task, confirm:

```
Task A-010 (HDR Output Detection) has been completed. Please verify:

1. Does the log show HDR detection with output capabilities (on HDR-capable system)?
2. Are supported color spaces enumerated correctly?
3. Can HDR color space be set successfully if display supports it?
4. Are there no errors or crashes during HDR detection on both SDR and HDR systems?

Reply with:
- "Confirmed - proceed to next task" if all checks pass
- "Issues found: [description]" if problems are discovered
- "Needs more testing" if additional verification is required
```

## 9. Related Issues

### Blocks
None - Feature detection only (no rendering changes)

### Related
- **A-009:** Tearing support detection (similar pattern of querying display capabilities)
- **A-001:** Hardcoded feature level (related to capability detection architecture)

## 10. Implementation Priority

**Priority:** P1-Medium
**Estimated Effort:** 4-5 hours
- 1 hour: Add HDR state tracking variables and methods
- 2 hours: Implement detectHDRCapabilities() with IDXGIOutput6 queries
- 1 hour: Implement setHDRColorSpace() for configuration
- 30 minutes: Integrate detection into swapchain creation/resize
- 30 minutes: Testing and validation

**Risk Assessment:** Low
- Detection-only feature (no rendering behavior changes)
- Graceful fallback to SDR if HDR not available
- IDXGIOutput6 availability checked before use
- No performance impact (detection only at swapchain creation)

**Impact:** Medium (Future-Proofing)
- Enables future HDR rendering support
- Provides visibility into display capabilities
- No immediate visual changes (requires application-level HDR implementation)
- Foundation for HDR feature development

## 11. References

- https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgioutput6-getdesc1
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-checkcolorspacesupport
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-setcolorspace1
- https://learn.microsoft.com/en-us/windows/win32/api/dxgicommon/ne-dxgicommon-dxgi_color_space_type
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HDR
