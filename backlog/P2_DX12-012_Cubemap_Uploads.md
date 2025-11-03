# Task: Implement cubemap face uploads

Context
- Issue IDs: DX12-012, DX12-113
- Symptom: `Texture::uploadCube` unimplemented; cannot populate cubemaps.
- Code refs: `src/igl/d3d12/Texture.cpp:372-392` (approx)

What to deliver
- Subresource mapping with `D3D12CalcSubresource` for 6 faces × mips.
- `CopyTextureRegion` with footprints; row/array pitch handling.
- Validation plan: render a skybox and compare vs Vulkan/GL; readback CRCs.

Constraints
- Support compressed and uncompressed formats; document limitations.

References
- CopyTextureRegion: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion
- D3D12CalcSubresource: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-d3d12calcsubresource
- Sample: HelloTexture cubemap variants; MiniEngine Texture loading

Acceptance Criteria
- Cubemap uploads succeed across mips; visual match with reference; tests pass.

