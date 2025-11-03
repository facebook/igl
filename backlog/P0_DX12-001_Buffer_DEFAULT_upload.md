# Task: Implement Buffer::upload for DEFAULT heap (staging path)

Context
- Issue IDs: DX12-001, DX12-101
- Symptom: `Buffer::upload` returns Unimplemented for DEFAULT-heap buffers; runtime updates fail.
- Code refs: `src/igl/d3d12/Buffer.cpp:82`

What to deliver
- Design note: transient UPLOAD allocations + `CopyBufferRegion` into DEFAULT buffer, with correct `ResourceBarrier`s.
- Implementation proposal: allocator reuse strategy (ring or pool), lifetime tied to command submission/fence.
- Pseudocode and function-level diffs for `Buffer::upload` and any helper/allocator plumbing.
- Risk/compat notes: alignment, size thresholds, small-copy coalescing; ensure no CPU stalls.
- Validation plan: unit tests for buffer updates, render test using GPU-only VB/IB/CB; DRED/validation layer clean.

Constraints
- Must not break existing tests or render sessions.
- Support FL11_0+; no reliance on RS v1.1 specifics.

References
- MS Learn – Uploading resources: https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources
- ID3D12GraphicsCommandList::CopyBufferRegion: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copybufferregion
- Barriers: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier
- Sample: D3D12HelloTexture, MiniEngine UploadBuffer

Acceptance Criteria
- Runtime updates to DEFAULT VB/IB/CB succeed; frame renders unchanged.
- No GPU validation errors; all tests pass.

