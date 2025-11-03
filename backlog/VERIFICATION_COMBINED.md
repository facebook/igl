# Verification Guide (Authoritative)

This is the authoritative verification guide for the DX12 backlog. It describes the mandatory harness and targeted, task-specific checks used to validate changes before commit.

## How To Use (Aligned With Orchestrator)

- For a single backlog task:
  1. Perform targeted verification first using the "Task-Specific Targeted Verification" section below (unit tests and/or render sessions appropriate for the feature).
  2. After fixing any targeted regressions, run the full reliability gate: `cmd /c mandatory_all_tests.bat` from repo root.
  3. Passing criteria: zero test failures; no GPU validation/DRED errors in logs; render sessions behave as expected.
  4. Artifacts to inspect:
     - Render: `artifacts\mandatory\render_sessions.log` and `artifacts\last_sessions.log`
     - Unit tests: `artifacts\mandatory\unit_tests.log` and `artifacts\unit_tests\D3D12\`
  5. On failure: fix forward and re-run; if destabilized, follow Failure Protocol in `backlog/ORCHESTRATOR_PROMPT.md` (soft stash or hard reset) and do not commit.
  6. On success: proceed with the orchestrator’s commit protocol (commit to current branch and remove the processed backlog markdown file).

## Combined Harness (mandatory_all_tests.bat)

- Entry point: `mandatory_all_tests.bat`.
- Execution: Run from the project root:
  - `cmd /c mandatory_all_tests.bat`
- Behavior:
  - Invokes `test_all_sessions.bat` and `test_all_unittests.bat` in sequence.
  - Captures each sub-suite’s console output under `artifacts\mandatory\render_sessions.log` and `artifacts\mandatory\unit_tests.log`.
  - Prints a summary indicating which suite passed or failed.
- When to use: Always run this first (or after targeted fixes) to establish current status across both render sessions and unit tests. If either side fails, consult the sections below.

## Render Sessions

- Entry point: `test_all_sessions.bat`.
- Execution: Run from the project root:
  - `cmd /c test_all_sessions.bat`
- Behavior:
  - Launches each required D3D12 render session (often concurrently).
  - Lets them run for the configured timeout (`TIMEOUT_SECONDS`, default 30s).
  - Forces termination of the corresponding `*_d3d12.exe` binaries after the timeout.
  - Scrapes the temporary log for each session and reports PASS if it rendered frames (`Frame 0` + `Signaled fence` markers) or FAIL with any errors.
- Artifacts:
  - Each run overwrites `artifacts\last_sessions.log` with the full console output.
  - Per-session logs are named `temp_<Session>.log` while the batch is running and are deleted afterward; consult `last_sessions.log` for preserved diagnostics.
- Failure triage:
  - Look at the “Collecting results for …” blocks in `artifacts\last_sessions.log`. Any render session that fails lists the reason and, when available, relevant error lines from the session log.
  - For deeper debugging: rerun the failing session manually from `build\vs\shell\Debug\<Session>_d3d12.exe` to reproduce interactively.

## Unit Tests (D3D12)

- Entry point: `test_all_unittests.bat`.
- Execution: Run from the project root:
  - `cmd /c test_all_unittests.bat`
- Behavior:
  - Configures and builds the test suite with D3D12 enabled; Vulkan/OpenGL are disabled.
  - Copies `dxil.dll` into the test output so DXC can sign DXIL during the run.
  - Executes `IGLTests.exe` with `--gtest_brief` and emits both log and XML artifacts.
- Artifacts (under `artifacts\unit_tests\D3D12`):
  - `IGLTests.log` — raw console output (all gtest sections, including failures and warnings).
  - `IGLTests.xml` — machine-readable gtest report.
  - `failed_tests.txt` — filtered list of failing tests (`[  FAILED  ] …` lines).
  - `configure.log`, `build.log` — CMake logs for setup and compilation.
- Failure triage:
  - Start with `failed_tests.txt` for the list of failing cases.
  - Use `IGLTests.log` to read surrounding messages (e.g., DXIL signing, assertions).
  - Parse `IGLTests.xml` if you need structured data or statistics.
  - Re-run a single test locally:
    - `build\vs\src\igl\tests\Release\IGLTests.exe --gtest_filter=<TestSuite>.<TestName>`

## Gotchas

- DXIL signing: The unit-test batch copies `dxil.dll` automatically. If the build directory is cleaned, rerun the script so the DLL is restored before launching `IGLTests.exe`.
- Parallel render runs: `test_all_sessions.bat` terminates sessions after the timeout. If you need a longer capture, adjust `TIMEOUT_SECONDS` in the script, but remember to kill the processes manually.
- Artifact locations: Render sessions write to `artifacts\last_sessions.log`; unit tests write under `artifacts\unit_tests\D3D12\`. Always inspect those paths for the latest run output.

## Task-Specific Targeted Verification (Supplemental)

Use these per-task checks to validate individual backlog items before the full harness.

- P0_DX12-001_Buffer_DEFAULT_upload
  - Unit: buffer upload/update tests (DEFAULT heap, `CopyBufferRegion`).
  - Render: GPU-only VB/IB/CB with dynamic constant updates; visual match expected.
- P0_DX12-002_CopyTextureToBuffer
  - Unit: upload→readback CRC across mip/array; include NPOT textures and compressed formats when available.
  - Render: blit and sample a pixel; confirm expected color.
- P0_DX12-003_RootSig_Feature_Bounds
  - Unit: RS v1.0 + Tier-1 creation with bounded tables succeeds; no device removal.
  - Render: bind multiple textures/buffers within bounded ranges; draw triangle.
- P0_DX12-004_Texture_Sampler_Limit
  - Unit/Render: bind/sample 8–16 textures/samplers; descriptors within capacity; output matches reference.
- P0_DX12-005_PushConstants_Graphics_Compute
  - Unit: verify values via `Set*Root32BitConstants`; CBV fallback matches.
  - Render/Compute: varying push ranges produce expected results.
- P0_DX12-011_Buffer_Bind_Groups
  - Unit: SRV/UAV/CBV buffer visibility with dynamic offsets.
  - Render/Compute: SRV+UAV chained passes produce expected output.
- P1_DX12-006_FeatureLevel_Probe
  - Unit: highest supported FL selected; FL11.x hardware doesn’t fall back to WARP.
- P1_DX12-007_CBV_Alignment
  - Unit: aligned offsets pass; unaligned auto-staged or explicit error; no silent rounding.
- P1_DX12-008_Descriptor_Heap_Management
  - Stress: churn without exhaustion; telemetry reports high-water; recycling verified.
- P1_DX12-009_Tearing_Gating
  - Run: vsync off + ALLOW_TEARING flag; vsync on without it; presents valid.
- P1_DX12-010_D3D12_Options_Tiers
  - Unit: caps cached; paths switch per tier; RS/table sizing respects tier.
- P1_DX12-111_Framebuffer_ArrayLayer_Subresource
  - Unit/Render: correct array-layer/cube-face subresource copies/resolves.
- P2_DX12-012_Cubemap_Uploads
  - Unit: 6×faces × mips footprints correct.
  - Render: skybox/probe visuals correct.
- P2_DX12-013_Upload_Manager_Pooling
  - Perf: bulk uploads faster; correctness unchanged; pooled allocators observed.
- P2_DX12-014_DSV_Readback
  - Unit: depth/stencil readback accuracy, including MSAA resolves.
- P2_DX12-015_Shader_Compiler_Fallback
  - Unit/Render: DXC vs FXC outputs match when features overlap.
- P2_DX12-016_WaitUntilScheduled
  - Unit: returns after queueing, not completion; no deadlocks.
- P2_DX12-017_Sampler_Destroy
  - Unit: sampler create/destroy loops reuse descriptors; no leaks.
- P2_DX12-018_Device_Limits_vs_Caps
  - Unit: enforced caps; no overcommit without downgrade/error.
- P2_DX12-021_022_BindBytes
  - Unit/Render/Compute: small constants via root constants or CBV read correctly.
- P2_DX12-120_Transient_Cache_Purging
  - Long-run: bounded transient memory across many submissions.
- P3_DX12-019_Debug_Labels
  - Tooling: markers visible in PIX; no overhead when disabled.
- P3_DX12-020_GPU_Timers
  - Unit/Perf: plausible timestamps and relative pass timings.

## Final Mandatory Step

- Execute `./mandatory_all_tests.bat` and confirm all tests pass (zero failures).
- If any targeted verification or the final gate fails, fix the regression or revert per the Failure Protocol in `backlog/ORCHESTRATOR_PROMPT.md`.
