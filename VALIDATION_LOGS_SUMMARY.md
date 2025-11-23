# D3D12 Validation Logs Summary

Scope: Validation output from the latest `mandatory_all_tests.bat` run executed with `IGL_D3D12_DEBUG=1`, `IGL_D3D12_GPU_VALIDATION=1`, `IGL_D3D12_DRED=1`, `IGL_DXGI_DEBUG=1`, and `IGL_D3D12_CAPTURE_VALIDATION=1`.

## Debug Layer / InfoQueue Overview

- Validation sources examined:
  - Console logs from:
    - `artifacts/mandatory/render_sessions.log`
    - `artifacts/mandatory/unit_tests.log`
    - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
    - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
  - InfoQueue dump captured by the D3D12 device destructor:
    - `artifacts/validation/D3D12_InfoQueue.log`
- The InfoQueue log now captures D3D12 debug‑layer messages (warnings and errors) that were previously only visible in the debugger. Messages are written whenever a D3D12 device is destroyed while `IGL_D3D12_CAPTURE_VALIDATION=1`.

## Render Sessions (D3D12)

- Source: `artifacts/mandatory/render_sessions.log`
- Result:
  - `PASSED: 16 sessions`
  - `FAILED: 0 sessions`
  - No `PresentManager: Present failed` messages.
  - No DRED breadcrumbs/page‑fault dumps from `D3D12PresentManager::logDredInfo`.
- InfoQueue:
  - No render‑session‑specific failures are called out in `D3D12_InfoQueue.log`, but the log is aggregated for all D3D12 devices, so session‑related warnings may be interleaved with unit‑test warnings.

## Unit Tests (D3D12)

- Harness summary: `artifacts/mandatory/unit_tests.log`
  - Debug configuration:
    - CMake and build succeed for D3D12 Debug/Release.
    - Both Debug and Release test runs complete.
  - Final status:
    - `UNIT TESTS: PASS` (no failing targets reported by the harness).
- Detailed gtest logs:
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
    - Summary: `[==========] 212 tests from 39 test suites ran.`, `[  PASSED  ] 212 tests.`
    - Previously failing `TextureLoaderFactoryTest.loadKtx2` now **passes** in D3D12 configuration.
  - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
    - Summary: gtest completes and `TextureLoaderFactoryTest.loadHdr` now **passes**; the Release run no longer exits early, and no failing tests are reported for D3D12.

## InfoQueue Message Groups (D3D12_InfoQueue.log)

File: `artifacts/validation/D3D12_InfoQueue.log`

- The log contains multiple `=== D3D12 InfoQueue Dump ===` sections, one per device destruction.
- Representative message groups (non‑exhaustive):
  - **ID 547 – API on closed command list**
    - Message: `ID3D12GraphicsCommandList::*: This API cannot be called on a closed command list.`
    - Indicates a D3D12 command‑list method was called after `Close()`. Probable sources include encoder paths that reuse command lists or attempt to record debug markers after closing.
  - **ID 820 – ClearRenderTargetView mismatch**
    - Message: `ID3D12CommandList::ClearRenderTargetView: The clear values do not match those passed to resource creation.`
    - This is a performance warning, not a correctness error, but per audit rules it is still considered a defect to be tracked.
  - **ID 646 – Uninitialized descriptors in descriptor table**
    - Message: `CGraphicsCommandList::SetGraphicsRootDescriptorTable: ... descriptor has not been initialized. All descriptors of descriptor ranges declared STATIC (not-DESCRIPTORS_VOLATILE) in a root signature must be initialized prior to being set on the command list.`
    - Repeated for multiple offsets in the same descriptor table. This suggests that at least one SRV descriptor table is being bound with partially or uninitialized entries (e.g., bind‑group or ResourcesBinder paths that assume dense population but leave gaps).

These InfoQueue findings did not cause visible test failures but represent D3D12 debug‑layer warnings/errors that must be triaged in subsequent tasks.

## Current Validation Status

- D3D12 debug layer, GPU‑based validation, DXGI debug, and DRED are now explicitly enabled for both headless (unit‑test) and windowed (render‑session) contexts via environment variables and in‑code wiring.
- All render sessions and unit tests **pass** under the current configuration.
- However, `artifacts/validation/D3D12_InfoQueue.log` shows multiple D3D12 debug‑layer errors and warnings. Under the audit rules, these mean the validation gate is **not yet clean**, even though the functional test suite passes.

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

