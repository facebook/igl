# A-009: Tearing Support Not Detected Properly

## 1. Problem Statement

**Severity:** MEDIUM
**Category:** A - Architecture
**Status:** Open
**Related Issues:** None

### Detailed Explanation

The current tearing support detection implementation has a critical gap: it only checks if DXGI supports tearing at the factory level, but does not verify that the specific swapchain actually supports tearing. This creates several issues:

1. **Incorrect Capability Reporting:** The application reports tearing as "supported" even when the current swapchain configuration prohibits it
2. **Missing Windowed Mode Check:** Tearing is only valid for windowed mode, but the code doesn't verify the swapchain was created in windowed mode
3. **No Swapchain Flag Verification:** After swapchain creation, the code doesn't verify `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` was actually set
4. **No Runtime Validation:** When using `DXGI_PRESENT_ALLOW_TEARING` in Present(), there's no validation that it's actually supported by the active swapchain
5. **Fullscreen Exclusion Missing:** Fullscreen exclusive mode prohibits tearing, but this constraint is not checked

### The Danger

- **Present() Failures:** Using `DXGI_PRESENT_ALLOW_TEARING` on incompatible swapchains causes Present() to fail with DXGI_ERROR_INVALID_CALL
- **Performance Regression:** Applications may enable vsync unnecessarily when tearing is actually available
- **Incorrect Diagnostics:** Debug logs report tearing as supported when it's not, misleading developers
- **Configuration Mismatch:** Swapchain flags may not match DXGI factory capabilities due to creation failures
- **User Experience:** Screen tearing may appear even when application believes it's disabled (or vice versa)

## 2. Root Cause Analysis

### Current Implementation

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.h`:

```cpp
// Line 116: Single boolean flag for tearing support
bool isTearingSupported() const { return tearingSupported_; }

// Line 218: No distinction between factory support and swapchain support
bool tearingSupported_ = false;
```

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp` (around line 564):

```cpp
void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  // ... setup swapchain desc ...

  // Check factory-level tearing support
  Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
  if (SUCCEEDED(dxgiFactory_.As(&factory5))) {
    BOOL allowTearing = FALSE;
    if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                &allowTearing,
                                                sizeof(allowTearing)))) {
      tearingSupported_ = (allowTearing == TRUE);  // ONLY CHECKS FACTORY
      if (tearingSupported_) {
        IGL_LOG_INFO("D3D12Context: Tearing support available\n");
      }
    }
  }

  // Set swapchain tearing flag if supported
  swapChainDesc.Flags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  // Create swapchain
  HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);

  // NO VERIFICATION: Did the swapchain actually support tearing?
  // NO CHECK: Is swapchain in windowed mode?
  // NO VALIDATION: Were the flags actually applied?
}
```

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\CommandQueue.cpp` (around line 527):

```cpp
// Lines 526-529: Uses tearing flag without runtime validation
if (ctx.isTearingSupported()) {
  presentFlags |= DXGI_PRESENT_ALLOW_TEARING;  // ASSUMES FLAG IS VALID
}

HRESULT presentHr = swapChain->Present(syncInterval, presentFlags);
// No check if failure was due to invalid tearing flag
```

### Technical Explanation

The root causes are:

1. **Factory vs Swapchain Confusion:** `CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING)` only checks if the DXGI factory supports the feature, not if the specific swapchain was created with the flag
2. **No Swapchain Query:** After creation, code doesn't call `GetDesc1()` to verify `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` is set
3. **Missing Windowed Mode Check:** Tearing requires windowed mode (`Windowed = TRUE` in `DXGI_SWAP_CHAIN_DESC1`), but this is not validated
4. **No Fullscreen Detection:** Code doesn't track fullscreen state changes via `IDXGISwapChain::SetFullscreenState()`
5. **Silent Swapchain Creation Failure:** If swapchain creation downgrades flags due to incompatibility, this goes undetected

## 3. Official Documentation References

1. **DXGI Variable Refresh Rate Displays (Tearing)**
   https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays
   *Key Guidance:* Tearing is only supported in windowed mode; verify swapchain flags after creation

2. **IDXGIFactory5::CheckFeatureSupport**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_5/nf-dxgi1_5-idxgifactory5-checkfeaturesupport
   *Key Guidance:* Checks if the system supports tearing, but application must still create swapchain with ALLOW_TEARING flag

3. **DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_chain_flag
   *Key Guidance:* Only valid for windowed swapchains; must be set at creation time

4. **IDXGISwapChain1::GetDesc1**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiswapchain1-getdesc1
   *Key Guidance:* Use to verify swapchain flags after creation

5. **DXGI_PRESENT_ALLOW_TEARING**
   https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present
   *Key Guidance:* Can only be used if swapchain was created with DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING

## 4. Code Location Strategy

### Files to Modify

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.h`
- **Search Pattern:** `bool isTearingSupported() const`
- **Context:** Tearing support query method around line 116
- **Action:** Add swapchain-specific tearing verification method and windowed mode tracking

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`
- **Search Pattern:** `CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING`
- **Context:** Tearing detection in createSwapChain() around line 564
- **Action:** Add post-creation swapchain flag verification and windowed mode check

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`
- **Search Pattern:** `CreateSwapChainForHwnd`
- **Context:** Swapchain creation around line 575
- **Action:** Verify swapchain flags after creation, log discrepancies

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`
- **Search Pattern:** `recreateSwapChain`
- **Context:** Swapchain recreation during resize
- **Action:** Re-verify tearing support after recreation

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\CommandQueue.cpp`
- **Search Pattern:** `if (ctx.isTearingSupported())`
- **Context:** Present flag determination around line 527
- **Action:** Add runtime validation before using DXGI_PRESENT_ALLOW_TEARING

## 5. Detection Strategy

### How to Reproduce

**Test Scenario 1: Fullscreen Swapchain**

```cpp
// Create swapchain in fullscreen mode
DXGI_SWAP_CHAIN_DESC1 desc{};
desc.Width = 1920;
desc.Height = 1080;
desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.BufferCount = 2;
desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;  // INVALID IN FULLSCREEN

DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc{};
fsDesc.Windowed = FALSE;  // Fullscreen mode

// Current implementation will report tearing as "supported"
// but Present() with DXGI_PRESENT_ALLOW_TEARING will fail
```

**Test Scenario 2: Swapchain Creation Downgrade**

```cpp
// Create swapchain with tearing flag on system that doesn't support it
// Factory reports support, but swapchain creation silently removes the flag

// Current implementation: isTearingSupported() returns true
// Reality: Swapchain doesn't have ALLOW_TEARING flag
// Result: Present() with tearing flag will fail
```

**Manual Testing:**

1. Run IGL application in fullscreen mode
2. Check if `isTearingSupported()` returns true
3. Attempt to disable vsync (use tearing)
4. Observe Present() failures or incorrect behavior

### Verification Steps

After implementing proper detection:

```bash
# Build with tearing diagnostics
cd C:\Users\rudyb\source\repos\igl\igl\build
cmake --build . --config Debug

# Run render session in windowed mode
cd C:\Users\rudyb\source\repos\igl\igl\build\shell\renderSessions\Debug
.\igl_shell_RenderSessions.exe --session=HelloWorld

# Expected log output:
# "D3D12Context: Factory supports tearing: YES"
# "D3D12Context: Swapchain created with tearing flag: YES"
# "D3D12Context: Swapchain mode: WINDOWED"
# "D3D12Context: Tearing support verified: YES"

# Set environment variable to test tearing
set IGL_D3D12_VSYNC=0
.\igl_shell_RenderSessions.exe --session=HelloWorld

# Verify no Present() errors and smooth rendering without vsync
```

## 6. Fix Guidance

### Implementation Steps

**Step 1: Add Swapchain State Tracking to D3D12Context.h**

Add after line 218 (`bool tearingSupported_ = false;`):

```cpp
// A-009: Detailed tearing support tracking
bool factorySupportsTearin g_ = false;     // Does DXGI factory support feature?
bool swapchainHasTearingFlag_ = false;   // Does swapchain have ALLOW_TEARING flag?
bool swapchainIsWindowed_ = true;         // Is swapchain in windowed mode?

// Helper to verify complete tearing support (factory + swapchain + windowed)
bool isSwapchainTearingSupported() const {
  return factorySupportsTearing_ &&
         swapchainHasTearingFlag_ &&
         swapchainIsWindowed_;
}

// Verify swapchain configuration after creation
void verifySwapchainTearingSupport();
```

**Step 2: Update isTearingSupported() in D3D12Context.h**

Replace line 116:

```cpp
// A-009: Use comprehensive check instead of factory-only check
bool isTearingSupported() const { return isSwapchainTearingSupported(); }
```

**Step 3: Implement Swapchain Verification in D3D12Context.cpp**

Add new function before `createSwapChain()`:

```cpp
void D3D12Context::verifySwapchainTearingSupport() {
  if (!swapChain_) {
    IGL_LOG_ERROR("D3D12Context::verifySwapchainTearingSupport() - No swapchain\n");
    swapchainHasTearingFlag_ = false;
    swapchainIsWindowed_ = false;
    return;
  }

  // Query actual swapchain flags
  DXGI_SWAP_CHAIN_DESC1 desc{};
  HRESULT hr = swapChain_->GetDesc1(&desc);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context::verifySwapchainTearingSupport() - GetDesc1 failed: 0x%08X\n", hr);
    swapchainHasTearingFlag_ = false;
    swapchainIsWindowed_ = false;
    return;
  }

  // Check if tearing flag is actually set
  swapchainHasTearingFlag_ = (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;

  // Check fullscreen state (tearing only valid in windowed mode)
  BOOL isFullscreen = FALSE;
  Microsoft::WRL::ComPtr<IDXGIOutput> output;
  hr = swapChain_->GetFullscreenState(&isFullscreen, output.GetAddressOf());
  if (SUCCEEDED(hr)) {
    swapchainIsWindowed_ = (isFullscreen == FALSE);
  } else {
    // GetFullscreenState can fail on some systems; assume windowed if creation used HWND
    swapchainIsWindowed_ = true;
    IGL_LOG_INFO("D3D12Context: GetFullscreenState failed, assuming windowed mode\n");
  }

  // Log comprehensive tearing support status
  IGL_LOG_INFO("D3D12Context: Tearing support verification:\n");
  IGL_LOG_INFO("  Factory supports tearing: %s\n", factorySupportsTearing_ ? "YES" : "NO");
  IGL_LOG_INFO("  Swapchain has tearing flag: %s\n", swapchainHasTearingFlag_ ? "YES" : "NO");
  IGL_LOG_INFO("  Swapchain is windowed: %s\n", swapchainIsWindowed_ ? "YES" : "NO");
  IGL_LOG_INFO("  Final tearing support: %s\n", isSwapchainTearingSupported() ? "YES" : "NO");

  // Warn if configuration is inconsistent
  if (factorySupportsTearing_ && !swapchainHasTearingFlag_) {
    IGL_LOG_ERROR("D3D12Context: Factory supports tearing but swapchain does NOT have flag!\n");
    IGL_LOG_ERROR("  This may indicate swapchain creation failure or incompatible configuration\n");
  }

  if (swapchainHasTearingFlag_ && !swapchainIsWindowed_) {
    IGL_LOG_ERROR("D3D12Context: Swapchain has tearing flag but is in FULLSCREEN mode!\n");
    IGL_LOG_ERROR("  Tearing is only valid in windowed mode; Present() will fail\n");
  }
}
```

**Step 4: Update createSwapChain() to Verify Support**

Replace the tearing detection code (around line 564):

```cpp
void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  // ... existing setup code ...

  // A-009: Check factory-level tearing support first
  Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
  if (SUCCEEDED(dxgiFactory_.As(&factory5))) {
    BOOL allowTearing = FALSE;
    if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                &allowTearing,
                                                sizeof(allowTearing)))) {
      factorySupportsTearing_ = (allowTearing == TRUE);
      IGL_LOG_INFO("D3D12Context: DXGI factory supports tearing: %s\n",
                   factorySupportsTearing_ ? "YES" : "NO");
    } else {
      factorySupportsTearing_ = false;
      IGL_LOG_INFO("D3D12Context: CheckFeatureSupport failed, assuming no tearing support\n");
    }
  } else {
    factorySupportsTearing_ = false;
    IGL_LOG_INFO("D3D12Context: IDXGIFactory5 not available, no tearing support\n");
  }

  // Configure swapchain desc
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.Width = width;
  swapChainDesc.Height = height;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.Stereo = FALSE;
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = kMaxFramesInFlight;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

  // A-009: Set tearing flag if factory supports it
  // Note: This may still fail if system doesn't support windowed tearing
  swapChainDesc.Flags = factorySupportsTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  // Create swapchain (windowed mode)
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
  HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(),
      hwnd,
      &swapChainDesc,
      nullptr,  // No fullscreen desc (windowed mode)
      nullptr,
      swapChain1.GetAddressOf());

  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: CreateSwapChainForHwnd failed: 0x%08X\n", hr);
    throw std::runtime_error("Failed to create D3D12 swapchain");
  }

  // A-009: Upgrade to IDXGISwapChain3 for modern APIs
  hr = swapChain1.As(&swapChain_);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: QueryInterface IDXGISwapChain3 failed: 0x%08X\n", hr);
    throw std::runtime_error("Failed to get IDXGISwapChain3 interface");
  }

  // A-009: CRITICAL - Verify swapchain actually has tearing support
  verifySwapchainTearingSupport();

  // Disable Alt+Enter fullscreen toggle (we manage fullscreen explicitly)
  dxgiFactory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

  // ... rest of initialization ...
}
```

**Step 5: Update recreateSwapChain() to Re-Verify Support**

Add verification call in `recreateSwapChain()` (around line 180):

```cpp
Result D3D12Context::recreateSwapChain(uint32_t width, uint32_t height) {
  // ... existing resize logic ...

  HRESULT hr = swapChain_->ResizeBuffers(
      kMaxFramesInFlight,
      width,
      height,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      factorySupportsTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: ResizeBuffers failed: 0x%08X\n", hr);
    return Result(Result::Code::RuntimeError, "Failed to resize swapchain");
  }

  // A-009: Re-verify tearing support after resize
  verifySwapchainTearingSupport();

  // Recreate render targets
  createBackBuffers();

  width_ = width;
  height_ = height;

  return Result();
}
```

**Step 6: Add Runtime Validation in CommandQueue.cpp**

Update Present() logic around line 527:

```cpp
// A-009: Runtime validation of tearing support before using flag
if (syncInterval == 0 && ctx.isTearingSupported()) {
  presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Using tearing (vsync disabled)\n");
#endif
} else if (syncInterval == 0 && !ctx.isTearingSupported()) {
  // Tearing requested but not supported - log warning
  IGL_LOG_ERROR("CommandQueue::submit() - VSync disabled but tearing NOT supported\n");
  IGL_LOG_ERROR("  Factory support: %s, Swapchain flag: %s, Windowed: %s\n",
                ctx.factorySupportsTearing() ? "YES" : "NO",
                ctx.swapchainHasTearingFlag() ? "YES" : "NO",
                ctx.swapchainIsWindowed() ? "YES" : "NO");
}

HRESULT presentHr = swapChain->Present(syncInterval, presentFlags);
if (FAILED(presentHr)) {
  IGL_LOG_ERROR("Present failed: 0x%08X\n", static_cast<unsigned>(presentHr));

  // A-009: Check if failure was due to invalid tearing flag
  if (presentHr == DXGI_ERROR_INVALID_CALL && (presentFlags & DXGI_PRESENT_ALLOW_TEARING)) {
    IGL_LOG_ERROR("  Possible cause: DXGI_PRESENT_ALLOW_TEARING used on incompatible swapchain\n");
    IGL_LOG_ERROR("  Swapchain tearing support: %s\n", ctx.isTearingSupported() ? "YES" : "NO");
  }

  // ... existing device removal check ...
}
```

**Rationale:**
- Factory check alone is insufficient; must verify swapchain flags post-creation
- Windowed mode is required for tearing; fullscreen check prevents invalid usage
- `GetDesc1()` provides ground truth for actual swapchain configuration
- Runtime validation in Present() catches configuration mismatches early
- Comprehensive logging enables quick diagnosis of tearing issues

### Before/After Code Comparison

**Before (factory-only check):**
```cpp
// Only checks if DXGI factory supports tearing
BOOL allowTearing = FALSE;
factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                             &allowTearing, sizeof(allowTearing));
tearingSupported_ = (allowTearing == TRUE);

// Assumes tearing works if factory supports it
if (ctx.isTearingSupported()) {
  presentFlags |= DXGI_PRESENT_ALLOW_TEARING;  // May fail!
}
```

**After (comprehensive verification):**
```cpp
// Check factory support
factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                             &allowTearing, sizeof(allowTearing));
factorySupportsTearing_ = (allowTearing == TRUE);

// Verify swapchain actually has the flag
DXGI_SWAP_CHAIN_DESC1 desc{};
swapChain_->GetDesc1(&desc);
swapchainHasTearingFlag_ = (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;

// Check windowed mode
swapChain_->GetFullscreenState(&isFullscreen, nullptr);
swapchainIsWindowed_ = !isFullscreen;

// Only report support if ALL conditions met
bool isTearingSupported() const {
  return factorySupportsTearing_ &&
         swapchainHasTearingFlag_ &&
         swapchainIsWindowed_;
}
```

## 7. Testing Requirements

### Unit Tests

**Command:**
```bash
cd C:\Users\rudyb\source\repos\igl\igl\build
ctest -C Debug -R "d3d12.*swapchain" --verbose
```

**Allowed Test Modifications:**
- Add new test `SwapchainTearingVerification` to verify post-creation flag check
- Add test `TearingFullscreenIncompatibility` to verify fullscreen mode detection
- Update existing swapchain tests to verify comprehensive tearing support

**New Test Cases to Add:**

```cpp
// In D3D12ContextTest.cpp
TEST(D3D12SwapchainTest, TearingVerification) {
  auto ctx = createWindowedContext();

  // Verify comprehensive tearing check
  bool factorySupport = ctx.factorySupportsTearing();
  bool swapchainFlag = ctx.swapchainHasTearingFlag();
  bool isWindowed = ctx.swapchainIsWindowed();

  // Factory support should match swapchain flag (on capable systems)
  if (factorySupport) {
    EXPECT_TRUE(swapchainFlag) << "Factory supports tearing but swapchain missing flag";
  }

  // Windowed mode should be true for CreateSwapChainForHwnd
  EXPECT_TRUE(isWindowed);

  // Overall tearing support should match all conditions
  bool expected = factorySupport && swapchainFlag && isWindowed;
  EXPECT_EQ(ctx.isTearingSupported(), expected);
}

TEST(D3D12SwapchainTest, ResizePreservesTearing) {
  auto ctx = createWindowedContext();
  bool tearingBefore = ctx.isTearingSupported();

  // Resize swapchain
  ctx.resize(1280, 720);

  // Tearing support should remain unchanged
  EXPECT_EQ(ctx.isTearingSupported(), tearingBefore);
}
```

### Render Session Tests

**Commands:**
```bash
cd C:\Users\rudyb\source\repos\igl\igl\build\shell\renderSessions\Debug

# Test tearing detection in windowed mode
.\igl_shell_RenderSessions.exe --session=HelloWorld --frames=100

# Expected log output:
# "D3D12Context: Tearing support verification:"
# "  Factory supports tearing: YES"
# "  Swapchain has tearing flag: YES"
# "  Swapchain is windowed: YES"
# "  Final tearing support: YES"

# Test with vsync disabled (tearing enabled)
set IGL_D3D12_VSYNC=0
.\igl_shell_RenderSessions.exe --session=HelloWorld --frames=100

# Should complete without Present() errors
# Expected: "CommandQueue::submit() - Using tearing (vsync disabled)"
```

## 8. Definition of Done

### Completion Criteria

- [ ] `factorySupportsTearing_` tracks DXGI factory capability separately
- [ ] `swapchainHasTearingFlag_` verified via `GetDesc1()` after swapchain creation
- [ ] `swapchainIsWindowed_` checked via `GetFullscreenState()`
- [ ] `isSwapchainTearingSupported()` returns true only if all three conditions met
- [ ] `verifySwapchainTearingSupport()` logs comprehensive status with all three checks
- [ ] `createSwapChain()` calls verification after successful creation
- [ ] `recreateSwapChain()` re-verifies support after resize
- [ ] `CommandQueue::submit()` validates tearing support before using `DXGI_PRESENT_ALLOW_TEARING`
- [ ] Error logs distinguish between factory support and swapchain configuration
- [ ] Warning logged if factory supports tearing but swapchain doesn't have flag
- [ ] Error logged if tearing flag present in fullscreen mode
- [ ] Unit tests verify comprehensive tearing detection logic
- [ ] Render sessions complete without Present() errors when tearing is enabled

### User Confirmation Required

Before proceeding to next task, confirm:

```
Task A-009 (Tearing Support Detection) has been completed. Please verify:

1. Does the log show comprehensive tearing verification with all three checks?
2. Is tearing correctly reported as unsupported in fullscreen mode?
3. Do render sessions work correctly with IGL_D3D12_VSYNC=0 (no Present errors)?
4. Are warnings logged if swapchain creation downgrades tearing flag?

Reply with:
- "Confirmed - proceed to next task" if all checks pass
- "Issues found: [description]" if problems are discovered
- "Needs more testing" if additional verification is required
```

## 9. Related Issues

### Blocks
None - Capability detection improvement only

### Related
- **A-010:** HDR output detection (similar pattern of verifying swapchain capabilities)
- **H-014:** Swapchain resize fallback (tearing support must be preserved across resize)

## 10. Implementation Priority

**Priority:** P1-Medium
**Estimated Effort:** 3-4 hours
- 1 hour: Add swapchain state tracking variables
- 1 hour: Implement `verifySwapchainTearingSupport()` with GetDesc1() query
- 1 hour: Update createSwapChain() and recreateSwapChain() to call verification
- 30 minutes: Add runtime validation in CommandQueue::submit()
- 30 minutes: Testing and validation

**Risk Assessment:** Low
- Changes are detection-only (no behavior modification)
- Existing code already attempts to use tearing correctly
- Verification adds diagnostic information without changing logic
- Easy to test by toggling vsync and checking logs

**Impact:** Low (Diagnostic Improvement)
- Improves debugging experience when tearing doesn't work as expected
- Prevents silent failures from incorrect tearing usage
- No performance impact (verification only at swapchain creation/resize)
- Users benefit from more accurate capability reporting

## 11. References

- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_5/nf-dxgi1_5-idxgifactory5-checkfeaturesupport
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_chain_flag
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiswapchain1-getdesc1
- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Fullscreen
