# Task: Query and use D3D12 Options and binding tiers

Context
- Issue IDs: DX12-010
- Symptom: Missing Options/Options1+ queries; binding tier unknown.
- Code refs: `src/igl/d3d12/Device.cpp:1750-1778`

What to deliver
- Query Options families; cache ResourceBindingTier, tiled resources, etc.
- Feed results into root/table sizing, feature toggles, and limits exposure.
- Validation plan: simulate tier differences (mock or adapters) and assert expected paths chosen.

Constraints
- Do not regress higher-tier behavior.

References
- D3D12 options: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature

Acceptance Criteria
- Caps exposed and used to guide behavior; tests pass across tiers.

