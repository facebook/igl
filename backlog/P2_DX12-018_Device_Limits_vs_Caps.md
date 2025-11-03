# Task: Validate engine limits against device caps

Context
- Issue IDs: DX12-018
- Symptom: Hard-coded limits not validated vs device capabilities.
- Code refs: `src/igl/d3d12/Common.h:26-37`, device caps query sites

What to deliver
- Compare requested limits (descriptor sets, samplers, vertex attribs) vs caps; adjust or error.
- Expose caps via device limits struct; align across backends.
- Validation plan: run across tiers; ensure no overcommit; existing content unaffected.

Constraints
- Non-breaking behavior for devices that meet prior assumptions.

References
- D3D12 options & tiers: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
- Vulkan-style limits mapping inspiration

Acceptance Criteria
- Limits reflect device caps; stable behavior; tests pass.

