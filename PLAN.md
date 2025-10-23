# IGL D3D12 Enablement Plan

Goal: Ship a complete Direct3D 12 backend for IGL that
- passes all unit tests in `src/igl/tests` on Windows,
- runs all desktop examples/samples,
- maintains API parity with other backends (OpenGL, Vulkan) and IGLU integration.

This PLAN is split into executable phases. Each phase has its own `PhaseX.md` with a ready‑to‑hand‑off implementation brief, code requirements, and acceptance tests. Common build, test, and run knowledge is centralized in `PLAN_COMMON.md`.

References used to author this plan (existing repo materials):
- D3D12 overview and deps: `third-party/d3d12/README.md`
- D3D12 implementation docs: `D3D12_IMPLEMENTATION_SUMMARY.md`, `DIRECTX12_MIGRATION_PLAN.md`, `DIRECTX12_MIGRATION_PROGRESS.md`
- Test infra reports: `tests/reports/d3d12_test_report.md`, `tests/reports/d3d12_test_execution_report.md`
- Engine structure notes: `PROJECT_STRUCTURE_ANALYSIS.md`, `PHASE_0_SUMMARY.md`, `PHASE_1_SUMMARY.md`, `PHASE_1_BUILD_FIXES.md`
- Existing backends (reference implementations):
  - OpenGL: `src/igl/opengl/**`
  - Vulkan: `src/igl/vulkan/**`
- Current D3D12 files: `src/igl/d3d12/**`
- RenderSessions matrix for D3D12 scope: `RENDERSESSIONS.md`

Cross‑validation strategy
- Parity-by-design: Map IGL concepts (Device, Buffer, Texture, Pipelines, CommandBuffer) to D3D12 following existing OpenGL and Vulkan behavior as ground truth.
- Test-first: Use `src/igl/tests/*` as conformance for features. For shader behavior, prefer identical test shaders across backends.
- Golden equivalence: For selected tests (e.g., Buffer, Texture upload, simple render), run on OpenGL/Vulkan and D3D12; compare observable results (return codes, byte counts, optional image hashes for offscreen runs).
- Capability alignment: Ensure `getTextureFormatCapabilities`, `hasFeature`, and `DeviceFeatureLimits` are consistent across backends for same hardware.

Phases (linked specs)
1. Phase 0 — Baseline & Infra: `Phase0.md`
2. Phase 1 — Device + Context Core: `Phase1.md`
3. Phase 2 — Resources & Views (Buffers, Textures, Samplers): `Phase2.md`
4. Phase 3 — Shaders & Pipelines (Graphics + Compute): `Phase3.md`
5. Phase 4 — Command Encoding & Framebuffer: `Phase4.md`
6. Phase 5 — Capabilities & IGLU Integration: `Phase5.md`
7. Phase 6 — Samples (Desktop) & Windowed Swapchain: `Phase6.md`
8. Phase 7 — Validation, CI & Parity Cross‑Checks: `Phase7.md`

See `PLAN_COMMON.md` for environment, build, test, and run instructions used across phases.
