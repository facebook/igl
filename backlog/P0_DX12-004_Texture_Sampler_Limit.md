# Task: Support up to 16 textures/samplers per stage

Context
- Issue IDs: DX12-004, DX12-109
- Symptom: Early return when `index >= 2`; violates `IGL_TEXTURE_SAMPLERS_MAX = 16`.
- Code refs: `src/igl/d3d12/RenderCommandEncoder.cpp:640-714`

What to deliver
- Plan to remove 2-slot guard; support 16 bindings with correct descriptor allocation and table updates.
- Heap capacity checks and failure handling; consider per-draw reuse of CPU->GPU descriptor copies.
- Diffs for binding code and any descriptor manager adjustments.
- Validation plan with shaders using 8–16 textures/samplers; stress test descriptor churn.

Constraints
- No perf regressions under typical material binding patterns.

References
- Descriptor heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps
- Samples: ExecuteIndirect descriptor table usage; MiniEngine descriptor management

Acceptance Criteria
- 16 bindings function correctly; no regressions in existing scenes/tests.

