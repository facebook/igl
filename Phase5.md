# Phase 5 — Capabilities & IGLU Integration

Objective
- Align D3D12 backend capabilities and feature limits with other backends.
- Make IGLU texture loader and helpers function correctly on D3D12.

Scope
- Capabilities: `hasFeature`, `hasRequirement`, `getTextureFormatCapabilities`, `DeviceFeatureLimits` audit.
- IGLU: texture loader creation paths, supported formats/usages on D3D12, small gaps filled in Texture utilities.

Design References
- Common: `PLAN_COMMON.md`
- IGLU texture loader: `IGLU/texture_loader/**`
- Capability logic in GL/Vulkan backends.

Acceptance Criteria
- IGLU texture loader tests pass (e.g., STB PNG/JPEG/HDR, KTX where supported).
- No capability asserts hit; capability queries match GL/Vulkan on same hardware where applicable.
 - RenderSessions validation (see RENDERSESSIONS.md):
   - Priority 7 session runs: TextureViewSession.
   - Progress on Priority 8 sessions (BindGroupSession, TinyMeshBindGroupSession, TinyMeshSession) as capabilities land.

Targeted Tests
- `BaseTextureLoaderTest.*`
- `Stb*TextureLoaderTest.*`
- `TextureLoaderFactoryTest.*`

Implementation Requirements
- Implement `isSupported(device, usage)` equivalents for D3D12 by accurately reporting texture usages and format support.
- Ensure any needed resource state transitions and copy paths exist to satisfy loader operations.

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- Updated capability methods and IGLU integration fixes.

Review Checklist
- Compare reported capabilities against Vulkan backend on same machine.
- Confirm loaders succeed/fail identically across backends for the same inputs.

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 5 for IGL D3D12: align capabilities and feature limits with existing backends and make IGLU texture loaders pass their tests by accurately exposing support and implementing necessary upload paths. Ensure `BaseTextureLoaderTest.*`, `Stb*TextureLoaderTest.*`, and `TextureLoaderFactoryTest.*` pass.
