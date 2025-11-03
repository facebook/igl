# Task: Probe and select highest supported D3D12 Feature Level

Context
- Issue IDs: DX12-006, DX12-107
- Symptom: Enforces FL12_0 for hardware; FL11.x adapters fall back to WARP.
- Code refs: `src/igl/d3d12/D3D12Context.cpp:171-210`

What to deliver
- Query `D3D12_FEATURE_FEATURE_LEVELS` and select highest supported for the adapter.
- Hardware vs WARP choice rules; logging and fallback sequencing.
- Validation plan: run on FL11_0/11_1/12_0/12_1 hardware (or sim via flags) and confirm no fallback to WARP.

Constraints
- Preserve existing adapter selection semantics otherwise.

References
- Feature levels: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_feature_levels
- Samples: HelloTriangle device creation paths

Acceptance Criteria
- Hardware adapters at FL11+ are used; tests/render sessions unaffected.

