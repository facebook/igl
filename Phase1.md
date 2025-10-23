# Phase 1 — Device & Context Core

Objective
- Complete core D3D12 Device and Context responsibilities to align with IGL semantics, using OpenGL/Vulkan backends as references.

Scope
- Context (windowless): device creation (finalize), command queue, descriptor heap managers, fence lifecycle.
- Device: replace stubs with correct behavior for capabilities, limits, basic resource query APIs.

Design References
- Common: `PLAN_COMMON.md`
- Reference backends: `src/igl/opengl/**`, `src/igl/vulkan/**`
- D3D12 device shell: `src/igl/d3d12/Device.{h,cpp}`
- Context: `src/igl/d3d12/D3D12Context.{h,cpp}`, `src/igl/d3d12/HeadlessContext.{h,cpp}`

Acceptance Criteria
- Accurate `getBackendVersion()`, `getBackendType()`, `getShaderVersion()` values.
- `hasFeature`, `hasRequirement`, `getTextureFormatCapabilities`, `getFeatureLimits` implemented to parity with GL/Vulkan where applicable.
- Stable descriptor heap managers exist for CBV/SRV/UAV, Sampler, RTV, DSV (CPU allocation + optional shader-visible for SRV/UAV/Sampler).
- No regressions in Phase 0 tests; additional capability‑centric tests pass.

Targeted Tests
- `BackendTest.*`
- `DeviceFeatureSetTest.*`
- `TextureFormatProperties.*` (capabilities/readouts only; not upload yet)
- `CommonTest.*`

Implementation Requirements
- Implement centralized managers for descriptor heap allocation (free‑list or bump with reset). Ensure thread-safety decisions match existing backends (single-threaded acceptable for now).
- Fill capability queries based on actual D3D12 feature levels and format support; consult GL/Vulkan return values for semantics.
- Maintain logging for feature detection and any unsupported paths.

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- Updated `Device.{h,cpp}` feature/limits methods.
- Descriptor heap manager classes or utilities.

Review Checklist
- Cross‑check capability returns vs Vulkan on the same machine.
- Unit tests for properties/capabilities pass.
- Heap allocators don’t leak and reset cleanly between tests.

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 1 for IGL D3D12: finish `Device` capability/limits methods to align with OpenGL/Vulkan semantics; add descriptor heap managers (CBV/SRV/UAV, Sampler, RTV, DSV). Ensure tests `BackendTest.*`, `DeviceFeatureSetTest.*`, and `TextureFormatProperties.*` pass. Keep logs clear and ensure no regressions in Phase 0.

