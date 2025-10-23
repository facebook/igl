# PLAN Common Knowledge (Build, Test, Run, Quality)

This file defines environment assumptions, unified build/test/run commands, coding requirements, review checklists, and cross‑validation guidance referenced by all phase documents.

Environment
- OS: Windows 11 (also Windows 10) x64
- Toolchain: Visual Studio 2022 (MSVC 19.4x) and CMake 3.25+
- SDK: Windows 10/11 SDK, DirectX 12 (d3d12.h, dxgi1_6.h), DXC preferred
- Optional: Vulkan SDK for cross‑backend reference runs
 - CI: Windows runners without guaranteed GPU; prefer hardware device creation then fall back to WARP automatically.

Build Configurations
- Debug and Release builds must compile.
- Minimal CMake switches for D3D12 bring‑up:
  - `-DIGL_WITH_TESTS=ON -DIGL_WITH_IGLU=ON -DIGL_WITH_D3D12=ON -DIGL_WITH_VULKAN=OFF -DIGL_WITH_OPENGL=OFF`

Canonical Build Commands
- Visual Studio (multi‑config):
  - `cmake -S . -B build\vs -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_TESTS=ON -DIGL_WITH_IGLU=ON -DIGL_WITH_D3D12=ON`
  - `cmake --build build\vs --config Debug --target IGLTests -j 8`

Running Tests
- Direct exe:
  - `build/<cfg>/src/igl/tests/IGLTests[.exe] --gtest_brief=1`
  - Focused run: `--gtest_filter=BufferTest.*`

Running Samples (after Phase 6)
- Desktop samples live under `samples/desktop/**` and depend on GLFW.
- For D3D12 windowed backend: create device from HWND, handle resize/present, run triangle/gltf examples.
- RenderSessions are first‑class samples and MUST be included: `shell/renderSessions/**`, plus `shell/apps/SessionApp.cpp`.
  - Scope: All RenderSessions that work on Vulkan MUST be supported and working on D3D12.
  - Validate these sessions on D3D12 with parity to OpenGL/Vulkan runs.

Coding Guidelines
- Never hack to pass a test; implement the API as designed, referencing OpenGL and Vulkan backends.
- Do NOT modify OpenGL or Vulkan backends, or common functionality, unless adding D3D12‑specific declarations/paths that do not alter existing semantics.
- Prefer small, composable classes (e.g., descriptor heap managers) with clear ownership/lifetime.
- Strict error handling: return `Result::Code` and log with sufficient context.
- Threading: Follow existing backends’ threading model; initial phases may assume single‑threaded command submission unless tests demand otherwise.
- Resource states: Always perform explicit transitions; centralize helpers for barriers.
 - Maintain repository code style (see `.clang-format`, `.cmake-format`); keep diffs minimal and consistent with existing patterns.
 - It is allowed to add equivalent HLSL shaders for samples/tests where needed.
  - Golden standard: OpenGL, Vulkan, and Meta implementations are the reference; do not change them to accommodate D3D12. D3D12 must conform.

Quality Gates (per phase)
- All targeted test suites for that phase must pass in Debug on hardware and WARP.
- No regressions in previously passing suites.
- Code is reviewed against OpenGL/Vulkan equivalents and matches IGL semantics.

Cross‑Validation (OpenGL/Vulkan vs D3D12)
- Behavioral comparison: For each implemented feature, run the same unit test(s) on OpenGL/Vulkan and D3D12 and compare:
  - Return codes and `Result::message`
  - Resource sizes, bytes per row/layer (for textures)
  - Optional: For simple offscreen draws, compare GPU outputs via CPU readback and hash.
 - Capability alignment: Verify `getTextureFormatCapabilities`, `hasFeature`, `DeviceFeatureLimits` match across backends where hardware permits.
 - Visual screenshot parity (later phases): capture offscreen renders/screenshots from RenderSessions and desktop samples, compare 1‑to‑1 against Vulkan baselines (byte‑exact). If capture scripts are unavailable, implement IGL-side capture to disk.

DXC Policy
- Primary: Use DXC (DirectX Shader Compiler, SM 6.x) when available for shader compilation. Benefits: modern HLSL features, better diagnostics, and future-proofing.
- Fallback: Use D3DCompile (SM 5.0) when DXC is not present, ensuring tests still compile and run on older SDKs.
- Implications:
  - Build/runtime dependency: Prefer dynamically locating DXC at runtime (dxcompiler.dll, dxil.dll). If absent, log and switch to D3DCompile automatically.
  - Feature parity: Unit tests and samples must avoid HLSL features unavailable on SM 5.0 when fallback is in effect, or provide equivalent SM 5.0 shaders.
  - Determinism: Ensure compile flags are fixed across runs; capture compile logs for CI triage.
  - Performance: DXC may produce different DXIL than D3DCompile’s DXBC; both must be functionally correct for tests/samples.

Screenshot Baselines & Image Comparison
- Baselines: Use Vulkan output as the reference (generate on the same machine/CI image when possible).
- Comparison: 1‑to‑1 byte equality for the final stages (no tolerance).
- Capture: Prefer existing scripts (e.g., PowerShell). If not viable, add IGL code paths to save render targets to disk for tests/sessions.
- Tooling: Use a single external comparison tool for development only (not committed to the repo). Configure its path via an environment variable; do not add multiple or ad‑hoc tools. For strict 1‑to‑1, a simple bytewise diff/hash is sufficient.

Artifacts & Deliverables (per phase)
- Code diffs strictly scoped to the phase’s files.
- Notes on which tests were unlocked and how they were validated.
- Brief doc updates where helpful (e.g., `third-party/d3d12/README.md`).



