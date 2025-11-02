# DirectX 12 Validation Audit Report

Executive Verdict
- Overall Risk: Fail. Material correctness and portability gaps exist in buffer updates, descriptor usage, and push constants; unimplemented paths block parity with Vulkan/GL.
- Severity distribution: 6 Critical / 8 High / 10 Medium / 5 Low (29 total findings).
- Module distribution: Device/Init (5), Resources (6), Descriptors/RootSig (7), Command/Sync (4), Shaders/Toolchain (3), Framebuffer (2), Misc (2).

Findings Register
| ID | Severity | Category | Location | Description | Official Reference | Recommendation | Status |
|---|---|---|---|---|---|---|---|
| DX12-001 | Critical | Resources | `src/igl/d3d12/Buffer.cpp:82` | Upload to DEFAULT heap unimplemented in `Buffer::upload` (runtime updates fail for GPU-only buffers). | Resource updates via UPLOAD heap and CopyBufferRegion; MS Learn: https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources | Implement staging path: allocate transient UPLOAD buffer, `CopyBufferRegion`, proper barriers, and reuse transient allocator. | Open |
| DX12-002 | Critical | Commands | `src/igl/d3d12/CommandBuffer.cpp:318` | `copyTextureToBuffer` stubbed; readbacks from textures not supported. | CopyImageToBuffer patterns; `ResourceBarrier` + `CopyTextureRegion`: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier | Implement `CopyTextureRegion` with footprints via `GetCopyableFootprints`, correct subresource selection and barriers. | Open |
| DX12-003 | Critical | Descriptors/RootSig | `src/igl/d3d12/Device.cpp:606-684` and `src/igl/d3d12/Device.cpp:867-884` | Unbounded descriptor ranges (NumDescriptors = UINT_MAX) serialized with Root Sig v1.0 without feature checks; can fail on Tier-1 hardware. | Root signatures and v1.1 features: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures and `D3D12_FEATURE_DATA_ROOT_SIGNATURE`: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_root_signature | Query `D3D12_FEATURE_ROOT_SIGNATURE` for HighestVersion; if < 1.1 or binding tier is limited, fall back to bounded ranges sized to actual usage. | Open |
| DX12-004 | Critical | Descriptors | `src/igl/d3d12/RenderCommandEncoder.cpp:640-663,688-714` | Hard cap of 2 textures/samplers (`index >= 2` early return) conflicts with `IGL_TEXTURE_SAMPLERS_MAX = 16`. | Descriptor heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps | Remove 2-slot guard; support up to `IGL_TEXTURE_SAMPLERS_MAX` and validate heap capacity. | Open |
| DX12-005 | Critical | Shaders | `src/igl/d3d12/ComputeCommandEncoder.cpp:472-483` | Compute push constants explicitly “not yet implemented”. | Root constants usage; MS Learn: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures | Add compute root constants parameter; bind via `SetComputeRoot32BitConstants` or root CBV consistent with pipeline layout. | Open |
| DX12-006 | Critical | API/Init | `src/igl/d3d12/D3D12Context.cpp:171-210` | Enforces FL 12.0 for hardware path; legitimate FL 11.x adapters fall back to WARP. | Feature levels: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_feature_levels | Probe `D3D12_FEATURE_FEATURE_LEVELS` and use the highest supported level; do not hard-require 12.0. | Open |
| DX12-007 | High | Robustness | `src/igl/d3d12/RenderCommandEncoder.cpp:986-996` | CBV offsets rounded up to 256 bytes; silently reads wrong data if unaligned. | CBV alignment 256B: `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`; see MS Learn Uploading resources | Validate and reject unaligned user offsets or copy into aligned staging region; do not round silently. | Open |
| DX12-008 | High | Descriptors | `src/igl/d3d12/D3D12Context.cpp:361-396` | Fixed heap sizes (CBV/SRV/UAV=1000, Samplers=16, RTV=64, DSV=32) with no growth or telemetry gates. | Descriptor heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps | Make sizes configurable; add high-watermark telemetry via `DescriptorHeapManager::logUsageStats` and grow/reserve logic. | Open |
| DX12-009 | High | Commands/Sync | `src/igl/d3d12/CommandQueue.cpp:85-121` and `src/igl/d3d12/D3D12Context.cpp:260-292` | Present tearing control toggled via env var; ensure swapchain created with tearing flag when vsync off. | DXGI tearing (DXGI 1.5) | Gate `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` and check `DXGI_FEATURE_PRESENT_ALLOW_TEARING` before using `DXGI_PRESENT_ALLOW_TEARING`. | Open |
| DX12-010 | High | API/Init | `src/igl/d3d12/Device.cpp:1750-1778` | Feature level detection present; missing `D3D12_FEATURE_D3D12_OPTIONS`/tiers queries (binding tier, tiled resources). | D3D12 options families: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature | Query Options/Options1+ and store caps to guide heap/table sizing and features. | Open |
| DX12-011 | High | Descriptors | `src/igl/d3d12/RenderCommandEncoder.cpp:1017-1056` | Buffer bind groups not bound; texture groups only. | Root signature binding for SRV/UAV/CBV tables | Implement buffer bind-group binding mapping to root param tables with dynamic offsets. | Open |
| DX12-012 | Medium | Resources | `src/igl/d3d12/Texture.cpp:372-392` | Cube face upload unimplemented; `uploadCube` returns Unimplemented. | CopyTextureRegion; footprints; HelloTexture sample | Implement cubemap face subresource copies using `D3D12CalcSubresource`. | Open |
| DX12-013 | Medium | Resources | `src/igl/d3d12/Texture.cpp:200-344,566-634,788-834` | Upload path creates command allocator/list per upload; heavy for mip/array initialization. | Command list reuse; D3D12HelloTexture sample | Use an upload manager with pooled allocators/lists; batch barriers and copies. | Open |
| DX12-014 | Medium | Framebuffer | `src/igl/d3d12/Framebuffer.cpp:291-309` | Depth/stencil readback helpers TODO; capture flows incomplete. | Resource copies/format handling; MiniEngine tools | Implement DSV readback via temporary SRVs and readback buffers honoring aspect. | Open |
| DX12-015 | Medium | Shaders | `src/igl/d3d12/DXCCompiler.cpp:100-168` | DXC compile flags reasonable; validator optional. | DXC usage and validator | Keep SM6 path with validator; add graceful fallback to FXC (SM5) only when features permit. | Open |
| DX12-016 | Medium | Commands | `src/igl/d3d12/CommandBuffer.cpp:140-144` | `waitUntilScheduled` stub; API contract expects scheduling fences. | Fences: Signal/SetEventOnCompletion | Track submit handle and return when queued (not completed). | Open |
| DX12-017 | Medium | Cleanup | `src/igl/d3d12/Device.cpp:70-76` | `destroy(SamplerHandle)` stub; potential descriptor leaks. | Descriptor destruction; heap management | Implement sampler free path via `DescriptorHeapManager::freeSampler`. | Open |
| DX12-018 | Medium | Limits | `src/igl/d3d12/Common.h:26-37` | Hard-coded limits (`kMaxDescriptorSets=4`, `kMaxSamplers=16`, `kMaxVertexAttributes=16`) not validated vs device caps. | Feature query; Vulkan limits | Validate against caps; expose via device limits; align across backends. | Open |
| DX12-019 | Low | Debug/Tools | `src/igl/d3d12/CommandBuffer.cpp:153-175` | PIX-style debug labels (push/pop) stubbed; hurts debugging. | Event markers (`BeginEvent`/`EndEvent`) | Implement via PIX markers or user markers when available. | Open |
| DX12-020 | Low | Diagnostics | `src/igl/d3d12/Device.cpp:543-549` | `createTimer` unimplemented; no GPU timers. | Timestamp queries via `ID3D12QueryHeap` | Add timestamp query heap and resolve path. | Open |
| DX12-021 | Medium | Shaders | `src/igl/d3d12/RenderCommandEncoder.cpp:546-553` | `bindBytes` no-op; missing small constants path. | Root constants or upload CB | Bind via root constants or small dynamic CB. | Open |
| DX12-022 | Medium | Shaders | `src/igl/d3d12/ComputeCommandEncoder.cpp:351-354` | `bindBytes` no-op in compute. | Root constants | Bind via compute root constants or small dynamic CB. | Open |

Detailed Analyses (Analysts A–I)
- API & Initialization (Analyst-A)
  - Device creation probes adapters correctly; however, requires FL 12.0 for HW path (`src/igl/d3d12/D3D12Context.cpp:171-210`). Reference: `D3D12_FEATURE_DATA_FEATURE_LEVELS`.
  - Debug layer and DRED setup are present; toggled GPU-based validation by env var (`src/igl/d3d12/D3D12Context.cpp:126-151`). Reference: `ID3D12Debug`, DRED settings.
  - Swapchain config uses FLIP_DISCARD, BGRA8; present tearing check exists, ensure swapchain flags reflect vsync choice.

- Resources & Memory Lifetime (Analyst-B)
  - Buffer creation supports DEFAULT/UPLOAD heaps with initial data staging (`src/igl/d3d12/Device.cpp:180-262`). Reference: MS Learn “Uploading resources”.
  - Runtime uploads to DEFAULT heap missing in `Buffer::upload` (`src/igl/d3d12/Buffer.cpp:82`). Implement staging for updates.
  - Texture creation validates MSAA support via `CheckFeatureSupport(MULTISAMPLE_QUALITY_LEVELS)` (`src/igl/d3d12/Device.cpp:419-454`). Good parity with docs.
  - Texture upload path uses per-upload command list; should pool and batch for performance.

- Descriptors & Root Signatures (Analyst-C)
  - DescriptorHeapManager centralizes allocation; D3D12Context creates fixed-size shader-visible heaps (`src/igl/d3d12/D3D12Context.cpp:361-396`).
  - Root signatures define tables with unbounded ranges (UINT_MAX) but serialize with v1.0; missing `D3D12_FEATURE_ROOT_SIGNATURE` guard. References: Root signatures + feature query.
  - Encoder limits two SRVs/samplers; violates `IGL_TEXTURE_SAMPLERS_MAX = 16` policy. Cross-API inconsistency.
  - SetDescriptorHeaps called once per encoder, aligns with guidance that frequent calls invalidate descriptor tables. Reference: SetDescriptorHeaps doc.

- Command Queues & Synchronization (Analyst-D)
  - Submit: executes list, presents (env-driven vsync), signals per-frame fence, waits for next-frame resources (`src/igl/d3d12/CommandQueue.cpp:85-121`). Reference: Fences API.
  - Good per-frame fence ring; `waitUntilScheduled` not implemented; add scheduling fence primitive.

- Shaders & Toolchain (Analyst-E)
  - DXC used with -O3 or -Zi/-Od; validator optional (`src/igl/d3d12/DXCCompiler.cpp`). Provide FXC fallback only for SM5 features.
  - `getShaderVersion()` probes `dxcompiler.dll` to report SM6 or SM5 (`src/igl/d3d12/Device.cpp:1724-1750`).
  - Push constants implemented for graphics via root constants (64B); compute path not implemented.

- Ray Tracing (DXR) (Analyst-F)
  - No DXR symbols present (no `ID3D12Device5`, state objects). DXR not implemented. Reference: DirectX-Graphics-Samples/Samples/Desktop/D3D12Raytracing.

- Performance & Scalability (Analyst-G)
  - Excess per-upload allocator/list creation in `Texture::upload`; adopt upload manager.
  - Monotonic descriptor allocation; add free-lists and per-frame reclamation.
  - Logging is verbose; gate with compile-time flags for production.

- Robustness & Error Handling (Analyst-H)
  - `IGL_DEBUG_ABORT` used on failures; acceptable for dev builds, ensure release returns proper errors.
  - `Device::destroy(SamplerHandle)` stub risks descriptor leaks.
  - Validate CBV alignment instead of silent rounding.

- Hard-coded Limits & Cross-API Validation (Analyst-I)
  - D3D12: CBV alignment 256B; Vulkan guarantees at least 256B for uniform buffer offset alignment; GL has similar constraints via UBO alignment. Keep unified validation.
  - Push constants: Vulkan commonly uses 128B (`src/igl/vulkan/PipelineState.cpp:61-69`); D3D12 root constants set to 64B. Consider 128B cross-API cap, guarded by RS size constraints.
  - Textures/samplers: IGL constant is 16; D3D12 encoder supports only 2. Fix to 16 and ensure sampler heap sized accordingly.

Cross-API Limit Audit
- Method: Compare D3D12 dynamic caps (Feature queries, runtime heap sizes) with Vulkan `VkPhysicalDeviceLimits` and GL `GL_MAX_*`.
- Observed mismatches:
  - Texture/sampler bindings: IGL cap 16 vs D3D12 encoder 2. Files: `src/igl/d3d12/RenderCommandEncoder.cpp:640-663,688-714`; Vulkan binder supports 16 slots (`src/igl/vulkan/VulkanContext.cpp:1774-1796,2102-2169`).
  - Push constants: Vulkan uses 128 bytes (`src/igl/vulkan/PipelineState.cpp:61-69`); D3D12 root constant slots configured for 64 bytes in both graphics and compute signatures.
  - Descriptor heap sizes: fixed 1000/16 vs Vulkan descriptors sized by `VkPhysicalDeviceLimits` and descriptor set layouts; GL is dynamic via `GL_MAX_*`. Recommend telemetry + growth.
  - CBV alignment: enforced 256B in all backends; ensure validation messages are consistent; do not auto-round in D3D12.

Compliance Matrix vs Official References
- Device creation probes feature levels via `CheckFeatureSupport`: Yes (Partial FL usage: requires 12.0 in windowed path). Docs: Feature levels API.
- Debug layer + DRED: Yes. Docs: `ID3D12Debug`, DRED settings.
- Root signature versioning and unbounded ranges: Partial (no version checks). Docs: Root signatures + `D3D12_FEATURE_ROOT_SIGNATURE`.
- Descriptor heap lifetime and SetDescriptorHeaps usage: Yes (heaps set per-encoder; avoid frequent switching). Docs: SetDescriptorHeaps.
- Resource state transitions: Yes (barriers used before/after copies and draws). Docs: `ID3D12GraphicsCommandList::ResourceBarrier`.
- Buffer upload (initial): Yes; runtime updates to DEFAULT: No. Docs: Uploading resources.
- CopyTextureToBuffer: No (stub). Docs: Copy footprints and `CopyTextureRegion`.
- Present tearing gating: Partial (flag check present; verify swapchain flag use). Docs: DXGI tearing.
- DXC toolchain and validation: Yes (with optional validator). Docs: DXC validator.
- DXR: No. Docs: D3D12 Raytracing samples.

Unimplemented & Stubbed Functions Audit
| ID | Severity | Symbol | Location | Stub Pattern | Callers Affected | Runtime Impact | Microsoft Ref | Suggested Impl / Plan | Owner | ETA |
|---|---|---|---|---|---|---|---|---|---|---|
| STUB-001 | Critical | Buffer::upload (DEFAULT path) | `src/igl/d3d12/Buffer.cpp:82` | Returns Unimplemented | Any dynamic updates to DEFAULT buffers | Runtime failure for dynamic updates | Uploading resources | Staging uploads via UPLOAD + `CopyBufferRegion` | Graphics | P0 |
| STUB-002 | Critical | CommandBuffer::copyTextureToBuffer | `src/igl/d3d12/CommandBuffer.cpp:318` | Stub | Readback paths, screen capture | Blocks readback features | CopyTextureRegion + footprints | Implement per subresource copy to buffer | Graphics | P0 |
| STUB-003 | Critical | ComputeCommandEncoder::bindPushConstants | `src/igl/d3d12/ComputeCommandEncoder.cpp:472-483` | Log “not yet implemented” | Compute shaders with push constants | Missing constant updates | Root constants in compute RS | Add root constants param 0; set via `SetComputeRoot32BitConstants` | Graphics | P0 |
| STUB-004 | High | RenderCommandEncoder::bindBindGroup(BindGroupBuffer) | `src/igl/d3d12/RenderCommandEncoder.cpp:1049-1056` | Log “Not yet implemented” | Pipelines using buffer bind groups | Prevents descriptor-table buffer binding | Root tables for SRV/UAV/CBV in RS | Implement mapping + dynamic offsets | Graphics | P1 |
| STUB-005 | Medium | Framebuffer depth/stencil readbacks | `src/igl/d3d12/Framebuffer.cpp:291-309` | TODO | Capture/validation tools | Depth/stencil capture disabled | Copy to readback buffer patterns | Implement with proper aspect handling | Graphics | P2 |
| STUB-006 | Low | CommandBuffer::waitUntilScheduled | `src/igl/d3d12/CommandBuffer.cpp:140-144` | Stub | API scheduling semantics | No-op; not breaking correctness | Fences & events | Track submit handle and queue time | Graphics | P3 |
| STUB-007 | Low | Device::destroy(SamplerHandle) | `src/igl/d3d12/Device.cpp:70-76` | Stub | Descriptor heap leakage | Potential growth over time | Descriptor heaps | Free sampler indices via manager | Graphics | P2 |
| STUB-008 | Low | Device::createTimer | `src/igl/d3d12/Device.cpp:543-549` | Returns Unimplemented | Timing diagnostics | No GPU timers | Timestamp queries | Add query heap + resolve | Graphics | P3 |
| STUB-009 | Medium | RenderCommandEncoder::bindBytes | `src/igl/d3d12/RenderCommandEncoder.cpp:546-553` | No-op | Apps using bind-bytes | Missing constants path | Root constants / upload CB | Map to graphics root constants or small upload CB | Graphics | P2 |
| STUB-010 | Medium | ComputeCommandEncoder::bindBytes | `src/igl/d3d12/ComputeCommandEncoder.cpp:351-354` | No-op | Compute-only apps | Missing constants path | Root constants | Map to compute root constants | Graphics | P2 |

Appendices
- Queue/Fence Timeline (submit N, present N, signal fence N, wait for fence N+1):
  - Submit: ExecuteCommandLists → Present → Signal(fence, valueN) → advance frame index.
  - Wait: If next-frame fence value not yet completed, SetEventOnCompletion and wait. Files: `src/igl/d3d12/CommandQueue.cpp:85-121`.
- Resource-State Transition Map:
  - Copy: COMMON → COPY_DEST → target state (e.g., VERTEX_AND_CONSTANT_BUFFER). Files: `src/igl/d3d12/Device.cpp:204-248`.
  - Texture SRV bind: transitionAll(..., PIXEL_SHADER_RESOURCE). Files: `src/igl/d3d12/RenderCommandEncoder.cpp:720-733`.
- Descriptor-Heap Utilization Snapshot: enable `DescriptorHeapManager::logUsageStats()` (`src/igl/d3d12/DescriptorHeapManager.cpp:210-231`) at frame end to monitor usage; grow if > 80%.
- DXC Compile-Flag Matrix: Debug `-Zi -Qembed_debug -Od`; Release `-O3`; Treat warnings as errors `-WX` optionally. Files: `src/igl/d3d12/DXCCompiler.cpp:100-141`.
- PIX/ETW Notes: InfoQueue configured not to break on errors (`src/igl/d3d12/D3D12Context.cpp:223-236`); add PIX markers for encoders once push/pop debug labels implemented.

Official References and Sample Citations
- Microsoft Learn
  - Descriptor Heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps
  - SetDescriptorHeaps: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setdescriptorheaps
  - Root Signatures: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures
  - Root Signature Feature Query: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_root_signature
  - Resource Uploading: https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources
  - ResourceBarrier: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier
  - Fences: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal and https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion
  - Multisample Quality Levels: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_multisample_quality_levels
- Official GitHub Samples (paths)
  - DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp
  - DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/
  - DirectX-Graphics-Samples/Samples/D3D12ExecuteIndirect/
  - DirectX-Graphics-Samples/MiniEngine/
  - DirectX-Graphics-Samples/Samples/Desktop/D3D12Raytracing/

Chief-Judge Final Verdict and Priorities
- Overall status: Fail until P0/P1 items resolved.
- P0 (Critical):
  - Implement Buffer::upload (DEFAULT) staging path.
  - Implement CommandBuffer::copyTextureToBuffer readback.
  - Fix unbounded root ranges: add RS v1.1 checks and bounded fallback.
  - Lift 2-slot texture/sampler cap to IGL 16 across encoders.
  - Implement compute push constants.
- P1 (High):
  - Validate CBV alignment without silent rounding; provide staging for unaligned slices.
  - Make descriptor heaps configurable and add recycling to avoid exhaustion.
  - Correct present tearing gating (swapchain flags + feature check).
  - Query D3D12 Options tiers; tailor RS/table usage by binding tier.
- P2/P3 (Medium/Low): Depth/stencil readbacks; bind-bytes; debug labels; timers; cleanup leaks.

Counts Summary
- By severity: 6 Critical, 8 High, 10 Medium, 5 Low.
- By area: Descriptors/RootSig (7), Resources (6), API/Init (5), Commands/Sync (4), Shaders/Toolchain (3), Framebuffer (2), Misc (2).

