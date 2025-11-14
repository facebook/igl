# Orchestrator Prompt: DX12 Feature Implementation Queue

Mission
- Sequentially implement DX12 backlog features one-by-one without regressions.
- Each feature change must pass the full validation gate before commit.
- After a successful implementation, commit the change and delete the feature request markdown.

Global Rules
- No parallel execution: run exactly one feature task at a time.
- Validation script is mandatory for ALL tasks: run `./mandatory_all_tests.bat` on Windows.
- Do not merge multiple features in a single commit. Keep changes minimal and scoped.
- Maintain compatibility with existing tests and render sessions.
- Prefer bounded, hardware-tier-safe solutions (support FL11.x+) as noted in tasks.

Model Directives
- Coding subagents must use: `gpt5-codex-high`.
- Orchestrator may remain as configured by the runner. If a coding subagent fails after a concrete attempt and triage, it may retry with the same model once fixes are applied. Do not downgrade model tier.
- Suggested decoding params for coding tasks: Temperature 0.1, Top_p 1.0.

Environment
- OS: Windows (PowerShell or CMD).
- Repo root assumed at this file’s parent directory.
- Tests: `mandatory_all_tests.bat` handles build + tests. Do not bypass it.

Validation Gate (must pass before commit)
- Use `backlog/VERIFICATION_COMBINED.md` for verification instructions
- Expected: zero test failures; no GPU validation or DRED errors in logs.
- Visual/render-session checks: if tests include sample renders, outputs must match baselines.

Commit Protocol (on success)
- Commit to the current branch (no branch-per-task).
- Stage and commit only the files required for the feature.
- Remove the processed backlog markdown file.
- Suggested commit message:
  - `DX12: Implement <Task Title> [<ID>]`
  - Body should summarize approach and link to Microsoft docs used.
- Example (PowerShell):
  - `git add -A`
  - `git rm <backlog-file>` (if not already removed)
  - `git commit -m "DX12: Implement <Task Title> [<ID>]"`

Failure Protocol (on failure)
- If tests fail or regressions are detected, choose revert strategy based on failure type:
  - Soft revert for inspectable issues (build/test failures without repo breakage): keep changes staged, fix forward OR stash for investigation: `git stash push -u -m "wip-<ID>"`.
  - Hard reset for destabilizing changes (environment broken, large regressions): `git reset --hard HEAD` and optionally `git clean -fd`.
- Log failure details and either fix within the same task iteration or move the task to the end of the queue (no commit).
- Do NOT commit partial or failing work.

Task Execution Template (Subagent Prompt)
- You are assigned exactly ONE task file from `backlog/`. Follow it precisely.
- Steps
  0) Model selection: use `gpt5-codex-high` for all coding work on this task. Use low temperature (0.1) and Top_p 1.0 for deterministic diffs and log reasoning.
  1) Read these references for context: `docs/DX12_Implementation_Plan.md`, your task file `<TASK_FILE>`, and `backlog/VERIFICATION_COMBINED.md`.
  2) Open the referenced code locations from the task and map the proposed changes.
  3) Implement the feature with minimal, well-scoped changes. Follow existing style and patterns.
  4) Ensure hardware-tier safety (FL11.x+, binding tiers) and fallbacks as specified.
  5) Targeted verification: run the specific unit tests and/or render sessions outlined for your task in `backlog/VERIFICATION_COMBINED.md`. Fix any regressions before proceeding.
  6) Full reliability gate: execute `./mandatory_all_tests.bat` from repo root. All tests must pass.
  7) If and only if both targeted verification and the full gate pass with no regressions:
     - Delete your task file `<TASK_FILE>`.
     - Commit with message: `DX12: Implement <Task Title> [<ID>]` to the current branch.
  8) If tests fail: choose soft or hard revert per Failure Protocol; record findings; do not commit.
- Constraints
  - Do not batch multiple backlog items.
  - Avoid code churn unrelated to the task.
  - Maintain behavior of existing render sessions and tests.

Ordered Task Queue (execute top-to-bottom)
- P0 (Must Fix)
  1) `backlog/P0_DX12-001_Buffer_DEFAULT_upload.md`
  2) `backlog/P0_DX12-002_CopyTextureToBuffer.md`
  3) `backlog/P0_DX12-003_RootSig_Feature_Bounds.md`
  4) `backlog/P0_DX12-004_Texture_Sampler_Limit.md`
  5) `backlog/P0_DX12-005_PushConstants_Graphics_Compute.md`
  6) `backlog/P0_DX12-011_Buffer_Bind_Groups.md`
- P1 (High)
  7) `backlog/P1_DX12-006_FeatureLevel_Probe.md`
  8) `backlog/P1_DX12-007_CBV_Alignment.md`
  9) `backlog/P1_DX12-008_Descriptor_Heap_Management.md`
  10) `backlog/P1_DX12-009_Tearing_Gating.md`
  11) `backlog/P1_DX12-010_D3D12_Options_Tiers.md`
  12) `backlog/P1_DX12-111_Framebuffer_ArrayLayer_Subresource.md`
- P2 (Important)
  13) `backlog/P2_DX12-012_Cubemap_Uploads.md`
  14) `backlog/P2_DX12-013_Upload_Manager_Pooling.md`
  15) `backlog/P2_DX12-014_DSV_Readback.md`
  16) `backlog/P2_DX12-015_Shader_Compiler_Fallback.md`
  17) `backlog/P2_DX12-016_WaitUntilScheduled.md`
  18) `backlog/P2_DX12-017_Sampler_Destroy.md`
  19) `backlog/P2_DX12-018_Device_Limits_vs_Caps.md`
  20) `backlog/P2_DX12-021_022_BindBytes.md`
  21) `backlog/P2_DX12-120_Transient_Cache_Purging.md`
- P3 (Diagnostics)
  22) `backlog/P3_DX12-019_Debug_Labels.md`
  23) `backlog/P3_DX12-020_GPU_Timers.md`

Per-Task Completion Checklist (subagent must confirm)
- Code compiles and `./mandatory_all_tests.bat` reports success.
- No GPU validation layer/DRED warnings introduced.
- No changes outside the task’s scope.
- Backlog file removed and commit created with the agreed format.

Notes for Subagents
- When a task mentions Microsoft docs or samples (MiniEngine, DirectX-Graphics-Samples), prefer the simplest, well-documented approach compatible with FL11.x devices.
- Keep changes reversible; avoid API surface changes unless explicitly required by the task.
- If a decision risks cross-backend behavior differences, document the rationale in the commit message.
