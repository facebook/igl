# Task: Implement depth/stencil readbacks

Context
- Issue IDs: DX12-014, DX12-112
- Symptom: DSV readback helpers are TODO; capture flows incomplete.
- Code refs: `src/igl/d3d12/Framebuffer.cpp:291-309`

What to deliver
- Strategies to read depth and stencil: create temporary SRVs/UAVs or copy to typeless + re-interpret.
- Use readback buffers with correct plane selection for D24S8/D32S8; handle resolve if MSAA.
- Validation plan: render depth patterns; readback compare to CPU-side depth generation.

Constraints
- Avoid undefined behavior for depth formats; follow MS guidance.

References
- Resource formats/planes: https://learn.microsoft.com/windows/win32/direct3d12/typed-unordered-access-view-loads
- Query footprints: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints
- MiniEngine tooling examples

Acceptance Criteria
- Accurate depth/stencil readbacks; passes tests; no validation errors.

