# Phase 6 — Samples (Desktop) & Windowed Swapchain

Objective
- Run all desktop samples with D3D12 by adding a windowed (HWND) context and swapchain present path.
 - Include RenderSessions as part of sample validation.

Scope
- `D3D12Context` windowed init: swapchain creation, resize, present, vsync toggle, backbuffer RTV/DSV.
- Integrate with GLFW windowing to acquire HWND.
- Ensure sample shader pipelines available (convert GLSL/SPIR-V to HLSL or maintain HLSL variants).
 - Validate `shell/renderSessions/**` sessions (e.g., `BasicFramebufferSession`, `TinyMeshBindGroupSession`, `ThreeCubesRenderSession`) under D3D12.

Design References
- Common: `PLAN_COMMON.md`
- Current `D3D12Context.{h,cpp}` windowed code path (to be completed)
- Samples structure: `samples/desktop/**`

Acceptance Criteria
- Hello Triangle runs and presents.
- Mesh/texture samples run (TinyMesh/gltf if present), with correct resize behavior.
- RenderSessions run successfully with D3D12 (all sessions that work on Vulkan must work on D3D12).
- Visual screenshot parity: for selected sessions/samples, produce screenshots that match OpenGL/Vulkan outputs 1‑to‑1 (byte‑exact or within defined tolerance per format).
 - RenderSessions validation (see RENDERSESSIONS.md):
   - Priority 2: HelloWorldSession runs with drawIndexed.
   - Priority 3: ThreeCubesRenderSession runs with uniform buffers.
   - Priority 4: TQSession and Textured3DCubeSession run with textures/samplers.
   - Priority 5: DrawInstancedSession runs with instancing.
   - Priority 6: MRTSession runs with multiple render targets.

Implementation Requirements
- Create swapchain with FLIP model; implement RTV heap for backbuffers; depth buffer management.
- Present path; synchronize with fences; handle resize events to recreate backbuffers.
- Shader availability: DXC or converted HLSL for sample content.
- Do not modify OpenGL/Vulkan backends or their sample shader code; introduce HLSL equivalents as needed.
 - Device selection in app/sample startup: attempt hardware FL 12.x device first, then fall back to WARP FL 11.0 automatically (log choice).

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- Completed windowed context and sample wiring for D3D12.

Review Checklist
- No device removal or present errors in logs; smooth resize.
- Visual correctness comparable to OpenGL/Vulkan samples.

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 6 for IGL D3D12: complete windowed `D3D12Context` (swapchain/present/resize) and run desktop samples (triangle + mesh/texture). Use GLFW to obtain HWND. Ensure parity of sample visuals and responsiveness.
