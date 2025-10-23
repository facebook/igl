# Phase 7 — Validation, CI & Cross‑Backend Parity

Objective
- Finalize D3D12 backend with comprehensive validation, CI integration, and parity cross‑checks versus OpenGL/Vulkan.

Scope
- CI jobs for Windows (hardware + WARP fallback) using Ninja single‑config to enable `ctest`.
- Cross‑backend execution of a curated subset of tests and (optionally) image‑hash comparisons for offscreen rendering.
- Performance sanity runs; descriptor heap sizing from env; memory leak checks.

Design References
- Common: `PLAN_COMMON.md`
- Existing GH workflows under `.github/workflows/**` (if any)

Acceptance Criteria
- All unit tests pass in Debug (and ideally Release) under D3D12.
- Samples run successfully in CI smoke (optional artifacts/screenshots).
- Cross‑backend checks show consistent behavior for selected tests.
- Final visual parity: Selected RenderSessions and desktop sample screenshots match OpenGL/Vulkan outputs (1‑to‑1) under controlled settings.

Targeted Tests
- Entire `src/igl/tests` suite enabled for D3D12.

Implementation Requirements
- Add CI matrix for backends (OpenGL/Vulkan/D3D12) on Windows; include WARP fallback.
- Provide a small conformance runner: run the same tests on GL/Vulkan and D3D12 and compare outputs; collect deltas for investigation.
- Add a screenshot comparison stage for RenderSessions and sample scenes. Use existing screenshot scripts when available; store baseline images and compare bytewise or with per‑format tolerance.
 - Baselines sourced from Vulkan runs; store baselines separately from the repo (or in CI artifacts) when running 1‑to‑1 comparisons to keep the repo clean.

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- CI configuration updated; documentation of parity results and known exceptions (if any).

Review Checklist
- No flaky tests; deterministic fence waits; no debug layer errors.
- Documentation updated (README/third‑party notes).

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 7 for IGL D3D12: integrate CI for Windows with Ninja single‑config, run full unit tests, enable a cross‑backend parity runner, and ensure samples smoke run. Document parity outcomes and finalize any straggling issues.
