# Task: Bind buffer SRV/UAV/CBV groups to root tables

Context
- Issue IDs: DX12-011, DX12-105
- Symptom: Buffer bind groups (SRV/UAV/CBV) not bound; only texture groups.
- Code refs: `src/igl/d3d12/RenderCommandEncoder.cpp:1017-1056`

What to deliver
- Mapping from engine buffer groups to root descriptor tables with dynamic offsets.
- Handling of mixed SRV/UAV/CBV and per-draw updates via CPU descriptor copies.
- Diffs in encoder binding, descriptor allocation, and validation.
- Validation plan: shaders consuming buffer SRV/UAV/CBV; compare results to Vulkan backend.

Constraints
- Maintain descriptor heap invariants and performance.

References
- Descriptor tables: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-tables
- Samples: ExecuteIndirect; MiniEngine compute/graphics buffer binding

Acceptance Criteria
- Buffer groups are functional and stable under multi-binding workloads; tests pass.

