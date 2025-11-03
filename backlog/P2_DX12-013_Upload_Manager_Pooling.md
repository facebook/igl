# Task: Pool command allocators/lists and batch upload work

Context
- Issue IDs: DX12-013, DX12-122
- Symptom: Per-subresource allocator/list & staging buffer creation; stalls and overhead.
- Code refs: `src/igl/d3d12/Texture.cpp:200-344,566-634,788-834`

What to deliver
- Upload manager design (pooled allocators, command lists, staging buffers); batch barriers/copies.
- Lifetime tied to fences; thread-safety considerations.
- Validation plan: measure CPU time for large mip/array uploads before/after; correctness unchanged.

Constraints
- Keep code paths simple for small uploads; avoid regressions.

References
- Samples: MiniEngine upload system patterns
- Uploading resources: https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources

Acceptance Criteria
- Reduced CPU time for bulk uploads; identical visual output; tests pass.

