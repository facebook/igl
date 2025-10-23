# Phase 3 — Shaders & Pipelines (Graphics + Compute)

Objective
- Provide robust shader compilation and pipeline creation:
  - DXC (SM 6.x) primary; D3DCompile (SM 5.0) fallback.
  - Graphics PSO and Compute PSO creation from IGL descriptors.

Scope
- `ShaderModule` from string/binary inputs; `ShaderLibrary` minimal support for tests.
- Root signature design matching IGL bind model (CBVs, SRV/UAV tables, Samplers, root constants for push constants).
- Graphics pipeline creation: input layout, raster/DS/depth, blend states, RTV/DSV formats.
- Compute pipeline creation.

Design References
- Common: `PLAN_COMMON.md`
- Existing D3D12 shader/module/pipeline skeletons: `src/igl/d3d12/ShaderModule.{h,cpp}`, `RenderPipelineState.{h,cpp}`
- Reference: Vulkan pipeline/shader flow.

Acceptance Criteria
- ShaderModule and Render/Compute pipeline tests pass for the shader stages used in unit tests.
- No shader compile errors for test shaders (with DXC or fallback to D3DCompile).
 - It is acceptable (and encouraged) to introduce equivalent HLSL shaders for samples/tests; do not alter GL/Vulkan shader code.

Targeted Tests
- `ShaderModuleTest.*`, `ShaderLibraryTest.*`
- `RenderPipelineStateTest.*` (where present), graphics pipeline creation in `DeviceTest.LastDrawStat`
- `ComputeCommandEncoderTest.*` (after Phase 4 bindings)

Implementation Requirements
- DXC integration when available; gracefully fall back to D3DCompile. Respect entry points and profiles (vs_6_0/ps_6_0/cs_6_0 or *_5_0 fallback).
- Root signature must expose resources expected by IGL tests (constant buffers, textures, samplers, UAVs where used).
- Input layout mapping from `VertexInputStateDesc` to D3D12 semantics.
 - Do not change OpenGL/Vulkan backends or common shader paths; confine changes to D3D12 code and additional HLSL sources.

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- Completed shader compilation paths and pipeline state creation.

Review Checklist
- Validate PSO/RootSignature creation with debug layer (no invalid bindings).
- Cross‑verify shader compilation for a few shaders on Vulkan (GLSL/SPIRV) vs HLSL DXC results (semantic equivalence in tests).

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 3 for IGL D3D12: finalize ShaderModule(+) with DXC primary and D3DCompile fallback; implement Render and Compute pipeline creation including root signature reflecting IGL binding model. Ensure ShaderModule/Library tests pass and graphics pipeline creation works for `DeviceTest` and related tests.
