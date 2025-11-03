# Task: Implement waitUntilScheduled using scheduling fences

Context
- Issue IDs: DX12-016, DX12-121
- Symptom: Stubbed; APIs expect scheduling fences.
- Code refs: `src/igl/d3d12/CommandBuffer.cpp:140-144`

What to deliver
- Track submit handle/fence; implement wait until queued (not completed) semantics.
- Interaction with per-queue fences; thread-safety and reentrancy.
- Validation plan: unit tests verifying non-blocking scheduling waits; render session stability.

Constraints
- Do not inadvertently wait for completion; avoid deadlocks.

References
- Fences: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal
- SetEventOnCompletion: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion

Acceptance Criteria
- Correct scheduling waits; no regressions in frame pacing; tests pass.

