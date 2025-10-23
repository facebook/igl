# RenderSessions Support Matrix (D3D12)

Scope rule: Any RenderSession that supports Vulkan (uses Vulkan/GLSL shader sources) MUST be supported on D3D12. The list below is derived from code analysis under `shell/renderSessions/**`.

Columns
- Session: Class/file name
- Uses GLSL/Vulkan: Presence of GLSL sources and/or `getVulkan*ShaderSource()`
- D3D12 HLSL Needed: Whether we must provide HLSL equivalents (or compile-time conversion) for parity
- Notes: Special handling, features, or dependencies

Sessions
- BasicFramebufferSession — Uses device/FB utilities; no explicit GLSL in file — HLSL Needed: Likely No — Notes: Offscreen framebuffer path — Required
- BindGroupSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Bind group semantics, textures/samplers
- ColorSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Color math variants
- DrawInstancedSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: Instancing
- EmptySession — No shaders — HLSL Needed: No — Notes: N/A — Required
- EngineTestSession — Uses OpenGL GLSL sources — HLSL Needed: Yes — Notes: Engine integration; port shader logic
- GPUStressSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: MultiView options; performance heavy
- HandsOpenXRSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: OpenXR path — Not required
- HelloOpenXRSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: OpenXR path — Not required
- HelloTriangleSession — Precompiled DXIL present — HLSL Needed: Already — Notes: D3D12 sample OK; ensure parity
- HelloWorldSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Baseline shading
- ImguiSession — Likely uses IGLU/imgui — HLSL Needed: Possibly — Notes: Verify pipelines
- MRTSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Multiple render targets
- Textured3DCubeSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Texture sampling
- TextureViewSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: Texture views
- ThreeCubesRenderSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Popular sample; good parity target
- TinyMeshSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: Mesh loading, descriptor binding
- TinyMeshBindGroupSession — Uses Vulkan GLSL sources — HLSL Needed: Yes — Notes: BindGroup variant
- TQSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: Testbed QA session
- TQMultiRenderPassSession — Uses OpenGL GLSL sources — HLSL Needed: Yes — Notes: Multiple passes
- YUVColorSession — Uses OpenGL + Vulkan GLSL sources — HLSL Needed: Yes — Notes: YUV sampling/convert — Not required

Implementation Priorities and Phase Mapping

- Priority 1 (Phase 4A: Minimal Encoding & Present)
  - BasicFramebufferSession — Features: CommandQueue/Buffer, RenderPass, Framebuffer create/update, RenderCommandEncoder.begin/end, Present.
  - EmptySession — Features: Same minimal encoding with clear; Present.

- Priority 2 (Phase 3 + 4B: Graphics Pipeline + Simple Draw)
  - HelloWorldSession — Features: Vertex/Index buffers, VertexInputState, ShaderStages (HLSL equivalents), RenderPipeline, bindVertex/Index, drawIndexed, depth attachment.

- Priority 3 (Phase 2 + 3 + 4C: Uniform Buffers)
  - ThreeCubesRenderSession — Features: ManagedUniformBuffer (CBV), per‑frame uniforms, depth, standard draw loop. Requires `bindUniform`/root CBVs.

- Priority 4 (Phase 2 + 3 + 4D: Textures & Samplers)
  - TQSession — Features: Texture sampling, SamplerState, bindTexture/bindSampler, fragment uniforms; HLSL equivalents.
  - Textured3DCubeSession — Features: Texture sampling, SamplerState; HLSL equivalents.

- Priority 5 (Phase 4E: Instancing)
  - DrawInstancedSession — Features: `drawIndexed` with instance count; viewport/scissor; ensure API supports instancing path.

- Priority 6 (Phase 4F: Multiple Render Targets)
  - MRTSession — Features: Multiple color attachments (RTVs), correct PSO `targetDesc` and OMSetRenderTargets for N RTTs.

- Priority 7 (Phase 5: Views, Capability Edge Cases)
  - TextureViewSession — Features: `createTextureView` across formats/layers/mips; SRV correctness.

- Priority 8 (Phase 5/6: Bind Groups & Complex Scenes)
  - BindGroupSession — Features: BindGroupTexture/Buffer creation and binding; descriptor tables; validate with sampling.
  - TinyMeshBindGroupSession — Features: BindGroup variant with mesh data; descriptor heap pressure; shader sampling.
  - TinyMeshSession — Features: Mesh loading, descriptor bindings, shader resources; good stress for descriptors.

- Optional/Not required per scope
  - HelloOpenXRSession, HandsOpenXRSession — OpenXR; defer.
  - YUVColorSession — YUV specific shaders; defer.

Notes
- HLSL Shader Strategy: Provide HLSL equivalents for sessions using GLSL (do not change GLSL). Keep to features available in SM 5.0 for D3DCompile fallback, or supply dual variants when needed.
- API Readiness: Each priority level lists the minimal API surface required to avoid pulling unimplemented features into an earlier phase.

Policy
- Do not modify OpenGL/Vulkan shader code; add HLSL equivalents (or provide runtime translation) for D3D12.
- Prioritize sessions also validated under Vulkan to maximize cross-backend parity.
- For screenshot parity, use Vulkan runs as baseline reference images.
