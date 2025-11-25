# D3D12 Test Results Matrix

This matrix compares the documented D3D12 unit-test baseline against the latest
`mandatory_all_tests.bat` run executed with D3D12 validation enabled (debug layer,
GPU-based validation, DRED, DXGI debug, InfoQueue capture).

## Summary Table

| Suite / Config                         | Source                                             | Total Tests | Passed | Failed | Skipped / Disabled | Notes |
|----------------------------------------|----------------------------------------------------|------------:|-------:|-------:|--------------------|-------|
| Unit tests – D3D12 (Debug, baseline)   | `artifacts/test_results/D3D12_UNIT_TEST_FINAL_REPORT.md` | 204        | 144    | 28     | 32 skipped         | Historical “final” D3D12 unit-test report (Nov 2, 2025); several failures documented (texture cubes, render encoder, loaders, etc.). |
| Unit tests – D3D12 (Debug, current)    | `artifacts/unit_tests/D3D12/IGLTests_Debug.log`    | 212        | 212    | 0      | 0                  | GTest summary: all 212 tests pass. Previously failing `TextureLoaderFactoryTest.loadKtx2` now passes. |
| Unit tests – D3D12 (Release, current)  | `artifacts/unit_tests/D3D12/IGLTests_Release.log`  | (not explicitly reported) | (no failures reported) | 0 | (N/A) | Release run now completes without failing `TextureLoaderFactoryTest.loadHdr`; no failing tests are reported in the harness. |
| Mandatory harness – Unit tests         | `artifacts/mandatory/unit_tests.log`               | (harness aggregate) | 1 target (D3D12) | 0 targets | N/A | Harness summary reports `UNIT TESTS: PASS` for the D3D12 configuration. |
| Render sessions – D3D12 (current)      | `artifacts/mandatory/render_sessions.log`          | 16 sessions | 16     | 0      | 0                  | All D3D12 render sessions build and execute successfully; summary: `PASSED: 16`, `FAILED: 0`. |

## Non-Regression Assessment

- **Unit tests**
  - Compared to the historical baseline (multiple failures), the current D3D12 Debug
    suite reports a clean pass (`212/212`). Texture loader tests that previously failed
    under D3D12 now pass.
  - The Release configuration also completes without reported D3D12 failures.
  - From a unit-test perspective, the non-regression gate is **GREEN**.

- **Render sessions**
  - Render sessions remain stable: `16/16` PASS, matching or improving the baseline
    regression logs under `artifacts/test_results`.
  - No new render-session failures were observed.

- **Validation and InfoQueue**
  - D3D12 debug layer, GPU-based validation, DXGI debug, and DRED are enabled for all
    tests via environment variables and context wiring.
  - The InfoQueue artifact (`artifacts/validation/D3D12_InfoQueue.log`) for this run:
    - Contains only `=== D3D12 InfoQueue Dump ===` headers and “no messages” notes.
    - Has **no non-waived D3D12 warnings or errors** (`ID=...` lines are absent).
  - Previously observed correctness issues (ID 547, 646) are fixed; remaining
    performance-only hints (IDs 820, 821, 677) are explicitly documented and waived in
    `VALIDATION_LOGS_SUMMARY.md` and `DX12_FIX_TASKS.md` with Microsoft Learn and
    DirectX-Graphics-Samples references.
  - Under the audit rules (“warning ⇒ defect unless fixed or justified”), these
    documented waivers are accepted, so the validation gate is **CLEAN**.

## Artifacts for This Run

- Harness logs:
  - `artifacts/mandatory/render_sessions.log`
  - `artifacts/mandatory/unit_tests.log`
- Unit-test logs (D3D12):
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.xml`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.xml`
- Validation logs:
  - `artifacts/validation/D3D12_InfoQueue.log`
- Historical / baseline context (unchanged in this run):
  - `artifacts/test_results/D3D12_UNIT_TEST_FINAL_REPORT.md`
  - `artifacts/test_results/final_unit_tests.log`
  - `artifacts/test_results/render_sessions_regression.log`

