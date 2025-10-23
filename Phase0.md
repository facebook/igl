# Phase 0 — Baseline & Infrastructure

Objective
- Ensure D3D12 tests start reliably (no early crashes), with a minimal headless D3D12 context usable by unit tests.
- Establish build/test flows and WARP fallback for CI.

Scope
- COM initialization in test entrypoint.
- Headless D3D12Context (no swapchain) creation path.
- Basic command queue, descriptor heaps, and fence.

Design References
- Common build/test guidance: `PLAN_COMMON.md`
- D3D12 deps: `third-party/d3d12/README.md`
- Current headless context files: `src/igl/d3d12/HeadlessContext.{h,cpp}` (introduced)
- Test harness: `src/igl/tests/CMakeLists.txt`, `src/igl/tests/main.cpp`

Acceptance Criteria
- IGLTests launches and lists tests under D3D12 backend without crashing.
- Focus tests like `DeviceTest.GetBackendType` pass on hardware and WARP.

Targeted Tests
- `DeviceTest.GetBackendType`
- `AssertTest.*`, `CommonTest.*` (startup smoke)

Implementation Requirements
- Do not rely on swapchain. Initialize device, command queue, CBV/SRV/UAV & Sampler heaps, and a fence.
- Adapter selection: try high‑performance FL 12.0; fallback to any FL 12.0; fallback to WARP FL 11.0.
- Enable debug layer when available; never break on messages when no debugger is attached.

Build/Run
- See `PLAN_COMMON.md` for Visual Studio commands and direct exe runs.

Deliverables
- `src/igl/tests/main.cpp` (COM init + signal handler)
- `src/igl/d3d12/HeadlessContext.{h,cpp}`
- CMake tweaks enabling tests on Windows+D3D12.

Review Checklist
- Test runs stable (no crash), logs show device created.
- WARP fallback validated.
- No changes to public API semantics.

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 0 for IGL D3D12: ensure `IGLTests.exe` starts and runs smoke tests on Windows with D3D12 using a headless context. Create D3D12 device (HW preferred, WARP fallback), command queue, minimal descriptor heaps, and a fence. Add COM init in `src/igl/tests/main.cpp`. Use Visual Studio builds and run the test executable directly. Do not add swapchain/present paths yet. Validate `DeviceTest.GetBackendType` passes and no startup crashes occur.
