# Phase 4 — Command Encoding & Framebuffer

Objective
- Implement full render and compute command encoding, resource bindings, and offscreen framebuffer path.

Scope
- `RenderCommandEncoder`: all binding methods (`bind*`), draw/drawIndexed, viewport/scissor, clear, OM RTV/DSV setup, state transitions around render passes.
- `CommandBuffer`/`CommandQueue`: allocator/list lifecycle, submit, fence sync.
- `Framebuffer`: create from attachments (color/depth), resolve MSAA where relevant.

Design References
- Common: `PLAN_COMMON.md`
- D3D12 encoders/command queue files: `src/igl/d3d12/RenderCommandEncoder.{h,cpp}`, `CommandBuffer.{h,cpp}`, `CommandQueue.{h,cpp}`, `Framebuffer.{h,cpp}`
- Vulkan/OpenGL encoders and FB implementations.

Acceptance Criteria
- Render encoder tests pass (basic drawing w/o swapchain, to offscreen RT).
- Compute encoder tests pass (after pipeline binding complete).
 - RenderSessions validation (see RENDERSESSIONS.md):
   - Priority 1 sessions run: BasicFramebufferSession, EmptySession.
   - Priority 2 initial draw path prepared (pipeline/encoders functioning for HelloWorldSession).

Targeted Tests
- `RenderCommandEncoderTest.*`
- `CommandBufferTest.*`
- `ComputeCommandEncoderTest.*`
- `FramebufferTest.*`

Implementation Requirements
- Ensure correct transitions for attachments (PRESENT isn’t used in headless; transition to/from RENDER_TARGET as needed).
- Implement binding of SRVs/UAVs/Samplers from bind groups into descriptor tables; handle root constants for push constants.
- Implement draw/dispatch paths and submission with fence wait to make tests deterministic.

Build/Run
- See `PLAN_COMMON.md`.

Deliverables
- Completed encoders/command queue/framebuffer behavior for offscreen rendering.

Review Checklist
- Debug layer shows no state transition or binding misuse.
- Offscreen renders produce expected results when compared to OpenGL/Vulkan tests (optional image hash).

Hand‑off Prompt (to implementation agent)
> Implement/verify Phase 4 for IGL D3D12: complete Render/Compute command encoding, bind groups (SRV/UAV/Sampler/CBVs), and offscreen framebuffer handling. Ensure `RenderCommandEncoderTest.*`, `CommandBufferTest.*`, `ComputeCommandEncoderTest.*`, and `FramebufferTest.*` pass without regressions.
