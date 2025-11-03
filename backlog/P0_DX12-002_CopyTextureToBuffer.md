# Task: Implement CommandBuffer::copyTextureToBuffer (texture readbacks)

Context
- Issue IDs: DX12-002, DX12-102
- Symptom: Readbacks from textures unsupported; stubbed path.
- Code refs: `src/igl/d3d12/CommandBuffer.cpp:318`

What to deliver
- Proposal for `CopyTextureRegion` readback flow using `GetCopyableFootprints` and a READBACK heap buffer.
- Subresource selection strategy (mip, array layer, plane), row pitch handling, and required barriers.
- Edge cases: compressed formats, MSAA resolve-before-readback, depth/stencil.
- Pseudocode + diffs touching CommandBuffer and helper structs.
- Validation plan: texture upload→readback CRC tests; render test sampling a read-back texture to verify content.

Constraints
- Preserve existing render sessions and tests; no format regressions.

References
- GetCopyableFootprints: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints
- CopyTextureRegion: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion
- Resource barriers: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier
- Samples: HelloTexture; MiniEngine Readback

Acceptance Criteria
- Accurate readbacks across formats/subresources; no GPU validation errors; tests pass.

