# Phase 6 — D3D12 Windowed Path, Sessions, and Pixel Parity (Refined Requirements)

This document refines the Phase 6 scope, quality bars, and deliverables for implementing a robust windowed D3D12 path and validating RenderSessions with strict pixel parity against Vulkan.

## Scope

- Implement and stabilize a windowed D3D12 path (HWND + flip-model swapchain) for desktop sessions.
- Only modify D3D12 backend code and session-specific HLSL branches as needed.
- Do not change OpenGL/Vulkan/common code or session sources unless explicitly approved.
- Honor the existing session vertex layouts, buffer indices, and attribute names used across other backends.

## Device and Swapchain

- Adapter selection: Prefer hardware adapter (FL 12.x). Fallback to WARP (FL 11.0) with clear logs.
- Swapchain: Flip model, BGRA8 UNORM (or UNORM_SRGB if supported consistently). Support optional tearing when vsync disabled.
- Resize: Release backbuffers/RTVs, `ResizeBuffers`, recreate RTVs and depth; set default viewport/scissor.

## Synchronization and State

- Maintain per-frame fences to avoid CPU/GPU hazards and ensure proper backbuffer reuse.
- Correct resource state transitions:
  - Swapchain backbuffer: `PRESENT` ↔ `RENDER_TARGET` (on begin/end of encoding respectively).
  - Offscreen attachments: `COMMON` ↔ `RENDER_TARGET`; when sampling: `PIXEL_SHADER_RESOURCE`.

## Framebuffer and Encoder Integration

- `Framebuffer::updateDrawable` must correctly bind swapchain backbuffer and offscreen textures.
- Encoder:
  - Bind RTV/DSV with `OMSetRenderTargets(N)`, where `N` is the number of color attachments.
  - Call `SetDescriptorHeaps` once per command list (CBV/SRV/UAV + Sampler) before binding descriptor tables; never pass null heaps.
  - Bind SRV descriptor table(s) (tN) and sampler table(s) (sN) before draw calls.
  - Support session-declared vertex bindings (slot indices/strides), including slot 1.

## HLSL Coverage

- Provide D3D12-compatible HLSL for sessions where GL/VK shaders are not directly usable.
- Ensure register usage aligns with root signature tables: `Texture2D : register(tN)`, `SamplerState : register(sN)`, `cbuffer : register(bN)`.

## Tests and Quality Gates

- After each major D3D12 backend change, run the full test suite; do not introduce new failures relative to the current D3D12 baseline.
- Logs must be clean: no `[IGL] Error` lines, no D3D12 debug-layer errors, no device removal.
- Stability: All approved sessions must run repeatedly without crashes.

## Pixel Parity with Vulkan

- Generate Vulkan baselines at 640x360 (windowed) and capture D3D12 outputs with identical parameters.
- Compare decoded PNG RGBA bytes (or raw RGBA8 dumps) for strict byte equality (1:1; no tolerance).
- Mark a session as “workable” only when it is stable (no errors, no crashes) and achieves exact pixel parity.

## Approved Sessions and Order

1. EmptySession
2. BasicFramebufferSession
3. HelloWorldSession
4. ThreeCubesRenderSession
5. Textured3DCubeSession
6. DrawInstancedSession
7. MRTSession

## Deliverables

- D3D12 code changes localized to backend and HLSL, preserving session contracts used by other backends.
- Vulkan baselines and D3D12 captures stored under `artifacts/baselines` and `artifacts/captures` with `meta.json` manifests.
- Parity report (from compare script) and a summary of session stability (no crashes/errors) and adapter fallback logs.

---

## Vulkan Baseline Capture Requirements

- Build (Vulkan only):
  - `cmake -S . -B build\vs_vulkan -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_VULKAN=ON -DIGL_WITH_SHELL=ON -DIGL_WITH_SAMPLES=ON -DIGL_WITH_OPENGL=OFF -DIGL_WITH_D3D12=OFF -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build\vs_vulkan --config Release --target ALL_BUILD -j 8`
- Sessions:
  - `EmptySession_vulkan.exe`
  - `BasicFramebufferSession_vulkan.exe`
  - `HelloWorldSession_vulkan.exe`
  - `ThreeCubesRenderSession_vulkan.exe`
  - `Textured3DCubeSession_vulkan.exe`
  - `DrawInstancedSession_vulkan.exe`
  - `MRTSession_vulkan.exe`
- Run windowed captures (remove `--headless` if not supported by platform):
  - Common args: `--disable-vulkan-validation-layers --screenshot-number 0 --viewport-size 640x360`
  - Output structure:
    - `artifacts/baselines/vulkan/<Session>/640x360/<Session>.png`
    - `artifacts/baselines/vulkan/<Session>/640x360/meta.json`
    - `meta.json` content example: `{ "backend":"Vulkan", "width":640, "height":360, "format":"RGBA8_UNORM", "frame":0 }`

## D3D12 Capture Requirements

- Build (D3D12 only):
  - `cmake -S . -B build\vs -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_D3D12=ON -DIGL_WITH_SHELL=ON -DIGL_WITH_SAMPLES=ON -DIGL_WITH_VULKAN=OFF -DIGL_WITH_OPENGL=OFF -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build\vs --config Debug --target ALL_BUILD -j 8`
- Sessions:
  - `EmptySession_d3d12.exe`
  - `BasicFramebufferSession_d3d12.exe`
  - `HelloWorldSession_d3d12.exe`
  - `ThreeCubesRenderSession_d3d12.exe`
  - `Textured3DCubeSession_d3d12.exe`
  - `DrawInstancedSession_d3d12.exe`
  - `MRTSession_d3d12.exe`
- Run captures (windowed by default; optionally `--headless` if supported):
  - Common args: `--screenshot-number 0 --viewport-size 640x360`
  - Output structure:
    - `artifacts/captures/d3d12/<Session>/640x360/<Session>.png`
    - `artifacts/captures/d3d12/<Session>/640x360/meta.json`

## Parity Comparison Requirements

- Dependencies: Python 3 + Pillow (`pip install Pillow`).
- Run: `python tools/screenshots/compare_images.py artifacts/baselines/vulkan artifacts/captures/d3d12`
- The script decodes PNGs to RGBA8 and performs strict byte-equality. It reports `[ OK ]` for exact matches and `[FAIL]` with the first differing byte or a size mismatch.

