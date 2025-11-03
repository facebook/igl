# Task: Root Signature feature checks and bounded ranges

Context
- Issue IDs: DX12-003, DX12-108
- Symptom: Unbounded descriptor ranges serialized with RS v1.0; Tier-1 devices may fail.
- Code refs: `src/igl/d3d12/Device.cpp:606-684,867-884`

What to deliver
- Capability detection plan using `D3D12_FEATURE_DATA_ROOT_SIGNATURE` and `D3D12_FEATURE_D3D12_OPTIONS` (ResourceBindingTier).
- Fallback design mapping to bounded ranges sized by actual usage; template rules for tables.
- Version negotiation (prefer 1.1; fallback 1.0 with bounds); serialization details.
- Testing matrix by binding tier (1–3) and FL 11/12.
- Validation plan: compile and run on Tier-1 hardware (or WARP mock), ensure no device removal.

Constraints
- No change in higher-tier behavior; must keep existing tests/render sessions intact.

References
- Root signatures: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures
- Feature query: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_root_signature
- D3D12 options & tiers: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
- Samples: MiniEngine root sig patterns

Acceptance Criteria
- RS creation succeeds across tiers; bounded fallback verified; tests/render sessions unaffected.

