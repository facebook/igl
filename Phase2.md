# Phase 2 — Resources & Views (Buffers, Textures, Samplers)

Objective
- Fully implement buffers, textures, and sampler states with correct memory heaps, state transitions, views, and upload paths.

Scope
- Buffers: DEFAULT/UPLOAD/READBACK, map/unmap, initial data staging, transitions to proper states.
- Textures: 2D/2DArray/Cube/3D, mip levels, sample count, DXGI mapping (incl. sRGB), SRV/RTV/DSV/UAV creation, copy paths.
- Samplers: full mapping from `SamplerStateDesc` to `D3D12_SAMPLER_DESC`.

Design References
- Common: `PLAN_COMMON.md`
- Buffer, Texture, Sampler in GL/Vulkan backends
- D3D12 resource docs; `src/igl/d3d12/Texture.{h,cpp}`, `Buffer.{h,cpp}`, `SamplerState.{h,cpp}`

Acceptance Criteria
- Buffer create/map/upload/readback unit tests pass.
- Texture upload/resize/view/format tests pass for supported formats.
- Sampler tests (binding later in Phase 4) at least construct with correct parameters.

Targeted Tests
- `BufferTest.*`
- `TextureTest.*`, `TextureFloatTest.*`, `TextureHalfFloatTest.*`
- `TextureArray*.*`, `TextureCube.*`
- `TextureFormat.*`, `TextureRangeDesc.*`, `TextureFormatProperties.*`

Implementation Requirements
- Respect constant buffer alignment (256 bytes) and general resource state transitions.
- DXGI format mapping table must align with IGL expectations (sRGB, depth/stencil, typeless where required for RTV/DSV/SRV combos).
- Implement staging via UPLOAD buffers when creating DEFAULT GPU resources with initial data.
- Implement `createTextureView` to produce SRV/UAV/RTV/DSV views based on `TextureViewDesc`.
- Implement copy helpers (buffer<->texture; texture<->texture) for test utilities.

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- Completed `Buffer`, `Texture`, `SamplerState` implementations with proper D3D12 calls.
- Format mapping/util files updated.

Review Checklist
- Compare bytes-per-row/layer calculations vs OpenGL/Vulkan returns.
- Validate transitions with debug layer enabled (no warnings/errors during tests).

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 2 for IGL D3D12: complete Buffer/Texture/Sampler implementations with correct heaps, mapping, transitions, and views. Ensure `BufferTest.*`, `Texture*.*`, and related format tests pass. Keep code aligned to Vulkan/OpenGL semantics and D3D12 best practices.

