# Task: Descriptor heap sizing, growth, recycling, and telemetry

Context
- Issue IDs: DX12-008, DX12-114, DX12-115
- Symptom: Fixed heap sizes; no growth/recycling; risk of exhaustion.
- Code refs: `src/igl/d3d12/D3D12Context.cpp:361-396`, encoder descriptor indices.

What to deliver
- Configurable initial sizes; high-watermark telemetry; growth strategy; free-list recycling.
- Overflow handling and logging; thread-safety considerations.
- Validation plan: stress tests with high descriptor churn; no crashes or leaks.

Constraints
- Maintain performance characteristics of common workloads.

References
- Descriptor heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps
- Samples: MiniEngine descriptor manager

Acceptance Criteria
- No exhaustion under typical workloads; telemetry reports usage; tests pass.

