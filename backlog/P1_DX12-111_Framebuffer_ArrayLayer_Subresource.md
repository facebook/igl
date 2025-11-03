# Task: Fix array-layer subresource selection in framebuffer copies/resolves

Context
- Issue IDs: DX12-111
- Symptom: Ignores array layer when computing subresources; breaks array/cube resolves.
- Code refs: `src/igl/d3d12/Framebuffer.cpp:361,365`

What to deliver
- Correct `D3D12CalcSubresource` usage for array slices and cube faces.
- Handle MSAA and plane aspects as needed; validate with arrays/cubes.
- Validation plan: copy/resolve tests for arrays and cubemaps.

Constraints
- Maintain existing non-array behavior.

References
- D3D12CalcSubresource: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-d3d12calcsubresource

Acceptance Criteria
- Correct copies/resolves for arrays/cubes; no regressions; tests pass.

