# D3D12 Test Results Matrix

This matrix compares the documented D3D12 unit‑test baseline against the latest `mandatory_all_tests.bat` run executed with D3D12 validation enabled.

## Summary Table

| Suite / Config | Source | Total Tests | Passed | Failed | Skipped / Disabled | Notes |
|----------------|--------|------------|--------|--------|--------------------|-------|
| Unit tests – D3D12 (Debug, baseline) | `artifacts/test_results/D3D12_UNIT_TEST_FINAL_REPORT.md` | 204 | 144 | 28 | 32 skipped | Historical “final” D3D12 unit‑test report (Nov 2, 2025); several failures documented (texture cubes, render encoder, loaders, etc.). |
| Unit tests – D3D12 (Debug, current) | `artifacts/unit_tests/D3D12/IGLTests_Debug.log` | 212 | 212 | 0 | 0 (or reported separately) | GTest summary: all 212 tests pass. Previously failing `TextureLoaderFactoryTest.loadKtx2` now passes. |
| Unit tests – D3D12 (Release, current) | `artifacts/unit_tests/D3D12/IGLTests_Release.log` | (not explicitly reported) | (no failures reported) | 0 (observed) | Release run now completes without failing `TextureLoaderFactoryTest.loadHdr`; no failing tests are reported in the harness. |
| Mandatory harness – Unit tests | `artifacts/mandatory/unit_tests.log` | (harness aggregate) | 1 target (D3D12) | 0 targets | N/A | Harness summary now reports `UNIT TESTS: PASS` for the D3D12 configuration. |
| Render sessions – D3D12 (current) | `artifacts/mandatory/render_sessions.log` | 16 sessions | 16 | 0 | 0 | All D3D12 render sessions build and execute successfully; summary: `PASSED: 16`, `FAILED: 0`. |

## Non‑Regression Assessment

- **Unit tests**
  - Compared to the historical baseline (many failures), the current D3D12 Debug suite now reports a clean pass (212/212). The specific texture‑loader tests that previously failed under D3D12 (`loadKtx2`, `loadHdr`) now succeed.
  - The Release configuration also completes without reported failures for D3D12.
  - From a pure unit‑test perspective, the non‑regression gate is **GREEN**.
- **Render sessions**
  - Render sessions remain stable: `16/16` PASS, matching or improving on the baseline regression logs under `artifacts/test_results`.
  - No new render‑session failures were observed.
- **Validation caveat**
  - The D3D12 InfoQueue log (`artifacts/validation/D3D12_InfoQueue.log`) now records debug‑layer warnings and errors (see `VALIDATION_LOGS_SUMMARY.md`). Under the audit rules, these validation messages are still considered open defects even though functional tests pass.

## Artifacts for This Run

- Harness logs:
  - `artifacts/mandatory/render_sessions.log`
  - `artifacts/mandatory/unit_tests.log`
- Unit‑test logs (D3D12):
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.xml`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.log`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.xml`
- Validation logs:
  - `artifacts/validation/D3D12_InfoQueue.log`
- Historical / baseline context (not regenerated in this run):
  - `artifacts/test_results/D3D12_UNIT_TEST_FINAL_REPORT.md`
  - `artifacts/test_results/final_unit_tests.log`
  - `artifacts/test_results/render_sessions_regression.log`

