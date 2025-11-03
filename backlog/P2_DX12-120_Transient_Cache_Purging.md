# Task: Purge transient buffer cache tied to fences

Context
- Issue IDs: DX12-120
- Symptom: `trackTransientBuffer` never purges; allocations accumulate across submissions.
- Code refs: `src/igl/d3d12/CommandBuffer.h:65-77`

What to deliver
- Fence-based retirement of transient allocations; ring allocator or pool with reclamation.
- Telemetry to observe high-water usage and lifetime.
- Validation plan: repeated submissions with transient usage; memory footprint remains bounded.

Constraints
- Thread-safe; minimal overhead on fast paths.

References
- MiniEngine transient allocator patterns
- Fences and GPU/CPU lifetime: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal

Acceptance Criteria
- No unbounded growth; tests pass; no leaks.

