# Task: Correct CBV offset alignment handling (no silent rounding)

Context
- Issue IDs: DX12-007, DX12-110
- Symptom: Offsets are rounded up to 256B; wrong data read.
- Code refs: `src/igl/d3d12/RenderCommandEncoder.cpp:986-996,1025`

What to deliver
- Validation path for 256B alignment; for unaligned slices, copy to aligned staging region.
- Error/return policy vs transparent staging; performance considerations.
- Validation plan: unit tests with aligned/unaligned offsets; frame correctness maintained.

Constraints
- Must not break existing CB usage; avoid perf regressions.

References
- Constant buffer alignment: `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`
- Uploading resources: https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources

Acceptance Criteria
- No silent misreads; tests and renders match pre-change output.

