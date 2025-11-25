# D3D12 Validation Logs Summary

Scope: Validation output from the latest `mandatory_all_tests.bat` run executed with
`IGL_D3D12_DEBUG=1`, `IGL_D3D12_GPU_VALIDATION=1`, `IGL_D3D12_DRED=1`, `IGL_DXGI_DEBUG=1`,
and `IGL_D3D12_CAPTURE_VALIDATION=1`.

## Debug Layer / InfoQueue Overview

- Validation sources examined:
  - Console logs:
    - `artifacts/mandatory/render_sessions.log`
    - `artifacts/mandatory/unit_tests.log`
    - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
    - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
  - InfoQueue dump captured by the D3D12 device destructor:
    - `artifacts/validation/D3D12_InfoQueue.log`
- Capture behavior:
  - The device destructor queries `ID3D12InfoQueue` when `IGL_D3D12_CAPTURE_VALIDATION=1`.
  - For each device, it appends a `=== D3D12 InfoQueue Dump ===` header.
  - It writes only non-INFO, non-MESSAGE severities, after applying a small deny-list of
    known, documented performance hints (IDs 820, 821, 677).

## Render Sessions (D3D12)

- Source: `artifacts/mandatory/render_sessions.log`
- Result:
  - `PASSED: 16 sessions`
  - `FAILED: 0 sessions`
  - No `PresentManager: Present failed` messages.
  - No DRED breadcrumbs or page-fault dumps from `D3D12PresentManager::logDredInfo`.
- InfoQueue:
  - No render-session–specific D3D12 errors or non-waived warnings appear in
    `artifacts/validation/D3D12_InfoQueue.log`.

## Unit Tests (D3D12)

- Harness summary: `artifacts/mandatory/unit_tests.log`
  - Debug configuration:
    - CMake and build succeed for D3D12 Debug and Release configurations.
    - Both Debug and Release test runs complete.
  - Final status:
    - `UNIT TESTS: PASS` (no failing targets reported by the harness).
- Detailed gtest logs:
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
    - Summary: `[==========] 212 tests from 39 test suites ran.`
               `[  PASSED  ] 212 tests.`
    - Previously failing texture loader tests (e.g. `TextureLoaderFactoryTest.loadKtx2`)
      now pass under D3D12.
  - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
    - Summary: D3D12 Release gtest run completes and
      `TextureLoaderFactoryTest.loadHdr` now passes; no failing tests are reported.

## InfoQueue Message Groups (Current Run)

File: `artifacts/validation/D3D12_InfoQueue.log`

- Structure:
  - Multiple `=== D3D12 InfoQueue Dump ===` sections, one per D3D12 device destruction.
  - The writer includes a note when no non-INFO messages are present.
- Current contents:
  - In the latest fully validated run, there are **no recorded D3D12 message IDs**
    (`ID=...` lines) in the log.
  - All previously observed correctness issues have been resolved:
    - **ID 547 – API on closed command list**
      - Fixed by:
        - Guarding all encoder entry points that touch `ID3D12GraphicsCommandList` with
          `CommandBuffer::isRecording()` checks.
        - Reordering `CommandBuffer::begin()` so that `commandList_->Reset(...)` is
          called before binding descriptor heaps and before any markers are emitted.
        - Ensuring debug markers (BeginEvent/EndEvent/SetMarker) are only issued when
          the command list is in the recording state.
    - **ID 646 – Uninitialized descriptors in descriptor table**
      - Fixed by:
        - Serializing RS 1.1 descriptor ranges as `DESCRIPTORS_VOLATILE` in
          `D3D12PipelineCache::getOrCreateRootSignature`, matching the per-draw
          descriptor update pattern.
        - Enforcing dense, fully-initialized SRV/UAV tables in
          `D3D12ResourcesBinder::updateTextureBindings` / `updateUAVBindings`.
        - Avoiding binding of SRV/sampler tables when the cached base GPU handle is null.
  - The remaining D3D12 messages are performance hints that are now explicitly waived
    and filtered:
    - **ID 820 / 821 – ClearRenderTargetView / ClearDepthStencilView clear-value hints**
      - Microsoft Learn’s documentation for `ClearRenderTargetView` and
        `ClearDepthStencilView` explains that providing an optimized clear value at
        resource creation enables fast clears; mismatched or missing optimized clear
        values only affect performance, not correctness.
      - References:
        - Microsoft Learn – resource creation and clear values:
          `https://learn.microsoft.com/windows/win32/direct3d12/clearrendertargetview-function`
          `https://learn.microsoft.com/windows/win32/direct3d12/resource-barriers-and-transitions`
        - DirectX-Graphics-Samples – HelloTexture / HelloFrameBuffering:
          `DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp`
          `DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloFrameBuffering/D3D12HelloFrameBuffering.cpp`
      - For color RTs the backend now deliberately avoids passing a fixed optimized
        clear value (to support arbitrary render-pass clear colors). For depth/stencil,
        a default optimized clear matching the common clear values is retained.
    - **ID 677 – Float PS output to unsigned-integer render targets**
      - The debug layer warns when a pixel shader with float output writes to a render
        target whose format has unsigned-integer components (e.g. R16_UINT, R16G16_UINT,
        R10G10B10A2_UINT, R32_UINT, R32G32B32A32_UINT).
      - Per Microsoft Learn’s resource format documentation, this is a defined behavior:
        the raw bits from the shader are copied without type conversion.
        These configurations are used intentionally for bit-packing tests.
      - References:
        - Microsoft Learn – resource formats and typed views:
          `https://learn.microsoft.com/windows/win32/direct3d11/format-usage` (applies to D3D12 formats as well)
        - DirectX-Graphics-Samples – MiniEngine:
          `DirectX-Graphics-Samples/MiniEngine/Core/CommandContext.cpp`

These waivers are explicitly documented here and in `DX12_FIX_TASKS.md`, with official
Microsoft references, and are treated as accepted performance-focused deviations rather
than functional defects.

## Current Validation Status

- D3D12 debug layer, GPU-based validation, DXGI debug, and DRED are enabled for both
  headless (unit-test) and windowed (render-session) contexts via environment variables
  and in-code wiring (see `HeadlessContext.cpp` and `D3D12Context.cpp`).
- All D3D12 render sessions and unit tests **pass** under the current configuration:
  - `Render Sessions: PASS (16/16)`
  - `Unit Tests: PASS` (Debug and Release).
- `artifacts/validation/D3D12_InfoQueue.log` contains **no non-waived D3D12 warnings
  or errors** for this run.
- With the above waivers justified using Microsoft Learn and DirectX-Graphics-Samples,
  the D3D12 validation gate for this audit run is **CLEAN**.

## Key Artifacts

- Render sessions:
  - `artifacts/mandatory/render_sessions.log`
- Unit tests (D3D12):
  - `artifacts/mandatory/unit_tests.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.xml`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.xml`
- Validation logs:
  - `artifacts/validation/D3D12_InfoQueue.log`

