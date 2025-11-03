DX12 Test Gap Plan (Unit Tests Only)

Scope
- Role: Technical Test Auditor (Strict). No implementation in this document.
- Goal: Close coverage gaps for missing D3D12 features by specifying deterministic, script‑runnable unit tests only (no new render sessions).
- Oracles: Vulkan and OpenGL unit runs produce goldens; D3D12 must match or fail deterministically.

Inventory
- Unit tests
  - Location: `src/igl/tests` (gtest). Key areas: render/compute encoders, framebuffer, textures (2D/arrays/cubes), buffers.
  - Run example: `build/.../src/igl/tests/Debug/IGLTests.exe --gtest_brief=1` (see `docs/D3D12_STATUS.md:199`).
  - Note: Render sessions and `test_all_d3d12_sessions.sh` are not used in this plan.

- Feature flags / CLI
  - GTest flags: `--gtest_filter`, `--gtest_output=xml:<path>`, `--gtest_brief=1`.
  - D3D12 env (optional per test): `IGL_D3D12_GPU_BASED_VALIDATION=1`; headless sizes via `IGL_D3D12_CBV_SRV_UAV_HEAP_SIZE`, `IGL_D3D12_SAMPLER_HEAP_SIZE`.

Global Test Conventions
- Offscreen image size for image‑based tests: `640x360`.
- Determinism: fixed geometry, seeds, inputs; no time‑dependent logic.
- Artifacts root: `artifacts/` with per‑test subfolders.
- Goldens: SHA256 of readbacks/images/logs recorded for Vulkan and OpenGL.

How To Produce Oracles (Vulkan and OpenGL)
- Vulkan unit‑test baselines
  - Configure: `cmake -S . -B build/vs_vulkan -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_VULKAN=ON -DIGL_WITH_OPENGL=OFF -DIGL_WITH_D3D12=OFF -DIGL_WITH_TESTS=ON`
  - Build: `cmake --build build/vs_vulkan --config Debug --target IGLTests -j 8`
  - Run: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=<Suite>.<Case> --gtest_output=xml:artifacts/baselines/vulkan/<Suite>.<Case>.xml`

- OpenGL unit‑test baselines
  - Configure: `cmake -S . -B build/vs_gl -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_OPENGL=ON -DIGL_WITH_VULKAN=OFF -DIGL_WITH_D3D12=OFF -DIGL_WITH_TESTS=ON`
  - Build: `cmake --build build/vs_gl --config Debug --target IGLTests -j 8`
  - Run: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=<Suite>.<Case> --gtest_output=xml:artifacts/baselines/opengl/<Suite>.<Case>.xml`

Artifacts and Manifest
- Per unit test (per backend)
  - XML: `artifacts/<group>/<backend>/<Suite>.<Case>.xml`
  - Log: `artifacts/<group>/<backend>/<Suite>.<Case>.log`
  - Optional image: `artifacts/<group>/<backend>/640x360/<Suite>.<Case>.png`
  - Optional hash: `artifacts/<group>/<backend>/<Suite>.<Case>.sha256` (or `hash.txt`)

- Manifest entry (example)
  - `name`, `backend`, and the artifact paths + `sha256` for images/hashes/logs.


Unit‑Only Feature Test Specifications

1) Buffer::upload to DEFAULT heap (GPU‑only) [DX12‑101]
- Test: BufferUpload.DefaultHeapDrawQuad
- Purpose: Exercise `Buffer::upload()` on DEFAULT heap (GPU‑only) VB/IB and validate by rendering a quad to an offscreen target.
- Preconditions: Create VB/IB with `ResourceStorage::Private`; call `upload()` to populate VB.
- Steps: Draw a solid centered quad; read back image; write SHA256.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=BufferUpload.DefaultHeapDrawQuad --gtest_output=xml:artifacts/features/buffer_upload/vulkan/BufferUpload.DefaultHeapDrawQuad.xml > artifacts/features/buffer_upload/vulkan/BufferUpload.DefaultHeapDrawQuad.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=BufferUpload.DefaultHeapDrawQuad --gtest_output=xml:artifacts/features/buffer_upload/opengl/BufferUpload.DefaultHeapDrawQuad.xml > artifacts/features/buffer_upload/opengl/BufferUpload.DefaultHeapDrawQuad.log 2>&1`
- Pass: D3D12 SHA256 equals goldens; no "Unimplemented".
- Flake: None (static scene).

2) copyTextureToBuffer (texture readback) [DX12‑102]
- Test: CopyTextureToBuffer.Readback
- Purpose: Verify `CommandBuffer::copyTextureToBuffer` using `GetCopyableFootprints` and correct subresource.
- Preconditions: Draw an 8×8 color pattern into a color target.
- Steps: Invoke readback; compute/write SHA256.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CopyTextureToBuffer.Readback --gtest_output=xml:artifacts/features/copy_texture_to_buffer/vulkan/CopyTextureToBuffer.Readback.xml > artifacts/features/copy_texture_to_buffer/vulkan/CopyTextureToBuffer.Readback.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CopyTextureToBuffer.Readback --gtest_output=xml:artifacts/features/copy_texture_to_buffer/opengl/CopyTextureToBuffer.Readback.xml > artifacts/features/copy_texture_to_buffer/opengl/CopyTextureToBuffer.Readback.log 2>&1`
- Pass: D3D12 hash equals goldens; logs clean.
- Flake: None.

3) Push constants – graphics [DX12‑103]
- Test: RenderCommandEncoder.PushConstantsTint
- Purpose: Validate PS push constants reach shader; tint white texture by (0.25, 0.5, 0.75, 1.0).
- Steps: Fullscreen draw; read pixel or image; write SHA256.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=RenderCommandEncoder.PushConstantsTint --gtest_output=xml:artifacts/features/push_constants/vulkan/RenderCommandEncoder.PushConstantsTint.xml > artifacts/features/push_constants/vulkan/RenderCommandEncoder.PushConstantsTint.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=RenderCommandEncoder.PushConstantsTint --gtest_output=xml:artifacts/features/push_constants/opengl/RenderCommandEncoder.PushConstantsTint.xml > artifacts/features/push_constants/opengl/RenderCommandEncoder.PushConstantsTint.log 2>&1`
- Pass: D3D12 equals goldens.
- Flake: None.

4) Push constants – compute [DX12‑104]
- Test: ComputeCommandEncoder.PushConstantsScale
- Purpose: Detect missing compute push constants by writing `out = base * 0.5` to a 64×64 texture.
- Steps: Dispatch; read back; write SHA256.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=ComputeCommandEncoder.PushConstantsScale --gtest_output=xml:artifacts/features/push_constants/vulkan/ComputeCommandEncoder.PushConstantsScale.xml > artifacts/features/push_constants/vulkan/ComputeCommandEncoder.PushConstantsScale.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=ComputeCommandEncoder.PushConstantsScale --gtest_output=xml:artifacts/features/push_constants/opengl/ComputeCommandEncoder.PushConstantsScale.xml > artifacts/features/push_constants/opengl/ComputeCommandEncoder.PushConstantsScale.log 2>&1`
- Pass: D3D12 equals goldens (expected fail until implemented).
- Flake: None.

5) Bind groups (buffers) [DX12‑105]
- Test: BindGroupBuffers.DynamicOffset
- Purpose: Validate uniform/storage buffer bind groups with dynamic offsets.
- Steps: Single UBO with two color entries; draw twice with offsets (0,N); sample left/right halves and verify expected colors.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=BindGroupBuffers.DynamicOffset --gtest_output=xml:artifacts/features/bind_groups/vulkan/BindGroupBuffers.DynamicOffset.xml > artifacts/features/bind_groups/vulkan/BindGroupBuffers.DynamicOffset.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=BindGroupBuffers.DynamicOffset --gtest_output=xml:artifacts/features/bind_groups/opengl/BindGroupBuffers.DynamicOffset.xml > artifacts/features/bind_groups/opengl/BindGroupBuffers.DynamicOffset.log 2>&1`
- Pass: D3D12 matches goldens.
- Flake: None.

6) Texture/sampler binding cap > 2 [DX12‑109]
- Test: RenderCommandEncoder.BindThreeTexturesSampleMix
- Purpose: Sample t0,t1,t2 and compose RGB; expect (1,1,1) with three 1×1 textures (R,G,B).
- Steps: Fullscreen draw; sample pixel; log/assert.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=RenderCommandEncoder.BindThreeTexturesSampleMix --gtest_output=xml:artifacts/features/descriptors/vulkan/RenderCommandEncoder.BindThreeTexturesSampleMix.xml > artifacts/features/descriptors/vulkan/RenderCommandEncoder.BindThreeTexturesSampleMix.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=RenderCommandEncoder.BindThreeTexturesSampleMix --gtest_output=xml:artifacts/features/descriptors/opengl/RenderCommandEncoder.BindThreeTexturesSampleMix.xml > artifacts/features/descriptors/opengl/RenderCommandEncoder.BindThreeTexturesSampleMix.log 2>&1`
- Pass: D3D12 equals goldens.
- Flake: None.

7) CBV offset validation (256‑byte alignment) [DX12‑110]
- Tests: CBVOffset.AlignedPass, CBVOffset.UnalignedFails
- Purpose: Ensure unaligned CBV offsets are rejected; aligned offsets render correctly.
- Steps: 512‑byte UBO; bind at 256 (pass) and 128 (fail); check error/result and rendering.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CBVOffset.* --gtest_output=xml:artifacts/features/cbv_offset/vulkan/CBVOffset.xml > artifacts/features/cbv_offset/vulkan/CBVOffset.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CBVOffset.* --gtest_output=xml:artifacts/features/cbv_offset/opengl/CBVOffset.xml > artifacts/features/cbv_offset/opengl/CBVOffset.log 2>&1`
- Pass: D3D12 matches goldens; unaligned is error (no silent rounding).
- Flake: None.

8) Array‑layer copies (framebuffer resolves) [DX12‑111]
- Test: ArrayLayerCopy.ColorAttachment
- Purpose: Verify array‑layer selection during copies.
- Steps: 2‑layer 2D array; write layer 1 magenta; copy to layer 0; read back; hash.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=ArrayLayerCopy.ColorAttachment --gtest_output=xml:artifacts/features/array_copy/vulkan/ArrayLayerCopy.ColorAttachment.xml > artifacts/features/array_copy/vulkan/ArrayLayerCopy.ColorAttachment.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=ArrayLayerCopy.ColorAttachment --gtest_output=xml:artifacts/features/array_copy/opengl/ArrayLayerCopy.ColorAttachment.xml > artifacts/features/array_copy/opengl/ArrayLayerCopy.ColorAttachment.log 2>&1`
- Pass: D3D12 hash equals goldens.
- Flake: None.

9) Depth/stencil readback [DX12‑112]
- Test: DepthStencil.ReadbackZ8
- Purpose: Validate depth/stencil attachment readback.
- Steps: Render a depth ramp; read back; hash.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=DepthStencil.ReadbackZ8 --gtest_output=xml:artifacts/features/depth_stencil/vulkan/DepthStencil.ReadbackZ8.xml > artifacts/features/depth_stencil/vulkan/DepthStencil.ReadbackZ8.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=DepthStencil.ReadbackZ8 --gtest_output=xml:artifacts/features/depth_stencil/opengl/DepthStencil.ReadbackZ8.xml > artifacts/features/depth_stencil/opengl/DepthStencil.ReadbackZ8.log 2>&1`
- Pass: D3D12 hash equals goldens.
- Flake: None.

10) Cubemap upload [DX12‑113]
- Test: TextureCube.UploadFacesAndSample
- Purpose: Verify `Texture::uploadCube` populates six faces.
- Steps: Set face colors; sample each face; verify/log; optional image hash.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=TextureCube.UploadFacesAndSample --gtest_output=xml:artifacts/features/cubemap/vulkan/TextureCube.UploadFacesAndSample.xml > artifacts/features/cubemap/vulkan/TextureCube.UploadFacesAndSample.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=TextureCube.UploadFacesAndSample --gtest_output=xml:artifacts/features/cubemap/opengl/TextureCube.UploadFacesAndSample.xml > artifacts/features/cubemap/opengl/TextureCube.UploadFacesAndSample.log 2>&1`
- Pass: D3D12 equals goldens.
- Flake: None.

11) Descriptor growth + telemetry [DX12‑114/115]
- Test: DescriptorTelemetry.HeadlessUsage (D3D12‑only)
- Purpose: Exercise descriptor allocation under constrained heaps and assert telemetry via `DescriptorHeapManager::logUsageStats()`.
- Steps: Set env heap sizes; allocate >64 CBV/SRV/UAVs and >8 samplers; log usage; save log.
- Commands
  - D3D12: `build/vs/src/igl/tests/Debug/IGLTests.exe --gtest_filter=DescriptorTelemetry.HeadlessUsage > artifacts/features/descriptors/d3d12/DescriptorTelemetry.HeadlessUsage.log 2>&1`
- Pass: D3D12 log shows usage lines for CBV/SRV/UAV, Samplers, RTVs, DSVs.
- Flake: None.

12) bindBytes (graphics + compute) [DX12‑118/119]
- Tests: RenderCommandEncoder.BindBytesTint, ComputeCommandEncoder.BindBytesStamp
- Purpose: Validate that `bindBytes` updates small uniform data without a dedicated buffer.
- Steps: Graphics tints a quad from 4‑float bytes; compute stamps a 32×32 region; read back; hash.
- Commands
  - Vulkan (graphics): `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=RenderCommandEncoder.BindBytesTint --gtest_output=xml:artifacts/features/bind_bytes/vulkan/RenderCommandEncoder.BindBytesTint.xml > artifacts/features/bind_bytes/vulkan/RenderCommandEncoder.BindBytesTint.log 2>&1`
  - OpenGL (graphics): `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=RenderCommandEncoder.BindBytesTint --gtest_output=xml:artifacts/features/bind_bytes/opengl/RenderCommandEncoder.BindBytesTint.xml > artifacts/features/bind_bytes/opengl/RenderCommandEncoder.BindBytesTint.log 2>&1`
  - Vulkan (compute): `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=ComputeCommandEncoder.BindBytesStamp --gtest_output=xml:artifacts/features/bind_bytes/vulkan/ComputeCommandEncoder.BindBytesStamp.xml > artifacts/features/bind_bytes/vulkan/ComputeCommandEncoder.BindBytesStamp.log 2>&1`
  - OpenGL (compute): `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=ComputeCommandEncoder.BindBytesStamp --gtest_output=xml:artifacts/features/bind_bytes/opengl/ComputeCommandEncoder.BindBytesStamp.xml > artifacts/features/bind_bytes/opengl/ComputeCommandEncoder.BindBytesStamp.log 2>&1`
- Pass: D3D12 equals goldens (expected fail until implemented).
- Flake: None.

13) Timers [DX12‑123]
- Test: Timer.CreateAndMeasure
- Purpose: Validate GPU timer availability and measurement API.
- Steps: Create timer; measure a small draw; assert success (Vulkan/GL) vs `Unimplemented` (D3D12 today).
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=Timer.CreateAndMeasure --gtest_output=xml:artifacts/features/timers/vulkan/Timer.CreateAndMeasure.xml > artifacts/features/timers/vulkan/Timer.CreateAndMeasure.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=Timer.CreateAndMeasure --gtest_output=xml:artifacts/features/timers/opengl/Timer.CreateAndMeasure.xml > artifacts/features/timers/opengl/Timer.CreateAndMeasure.log 2>&1`
- Pass: D3D12 currently `Unimplemented` recorded; Vulkan/GL have positive duration.
- Flake: Minor timing variance (assert invariants only).

14) waitUntilScheduled [DX12‑121]
- Test: CommandBuffer.WaitUntilScheduled_NoHang
- Purpose: Validate presence of scheduling fences; D3D12 impl is stubbed.
- Steps: Call `waitUntilScheduled()` before/after submit; assert no crash; record log.
- Commands
  - Vulkan: `build/vs_vulkan/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CommandBuffer.WaitUntilScheduled_NoHang --gtest_output=xml:artifacts/features/scheduling/vulkan/CommandBuffer.WaitUntilScheduled_NoHang.xml > artifacts/features/scheduling/vulkan/CommandBuffer.WaitUntilScheduled_NoHang.log 2>&1`
  - OpenGL: `build/vs_gl/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CommandBuffer.WaitUntilScheduled_NoHang --gtest_output=xml:artifacts/features/scheduling/opengl/CommandBuffer.WaitUntilScheduled_NoHang.xml > artifacts/features/scheduling/opengl/CommandBuffer.WaitUntilScheduled_NoHang.log 2>&1`
- Pass: D3D12 logs stub; Vulkan/GL return promptly.
- Flake: None.


Run Recipes (Unit tests)
- D3D12 target runs
  - Configure: `cmake -S . -B build/vs -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_D3D12=ON -DIGL_WITH_OPENGL=OFF -DIGL_WITH_VULKAN=OFF -DIGL_WITH_TESTS=ON`
  - Build: `cmake --build build/vs --config Debug --target IGLTests -j 8`
  - Run (example): `build/vs/src/igl/tests/Debug/IGLTests.exe --gtest_filter=CopyTextureToBuffer.Readback > artifacts/features/copy_texture_to_buffer/d3d12/CopyTextureToBuffer.Readback.log 2>&1`

Pass Criteria Summary
- No device removal/validation errors in logs.
- Visual/data tests: D3D12 SHA256 matches Vulkan/GL goldens.
- Behavior tests (timers, scheduling, telemetry): expected success/failure codes and expected log lines.

Flake Risks and Mitigations
- Rendering nondeterminism: offscreen, fixed inputs, no time dependence.
- Timer variance: assert invariants (created, finite, >0) not absolute times.
- D3D12‑specific telemetry: only validated on D3D12; Vulkan/GL marked N/A where applicable.

Proposed Manifest Seeds
- File: `artifacts/manifests/DX12_TEST_GAP_PLAN.json` — one entry per backend for each test with `sha256` placeholders to fill after Vulkan/GL baselines.

Appendix
- Existing helper scripts (not used here): `tools/screenshots/*` capture sessions; this plan relies solely on unit tests.

