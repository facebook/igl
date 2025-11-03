# DirectX 12 Implementation Plan (Consolidated)

Source documents combined and deduplicated:
- docs/DX12_Audit_Report.md (IDs DX12-001..022)
- docs/DX12_Audit_Report_second.md (IDs DX12-101..123)

Goals
- Close correctness and portability gaps vs Vulkan/GL backends.
- Maintain compatibility with existing tests and render sessions.
- Avoid destabilizing lower feature-tier hardware; provide bounded fallbacks.

Assumptions
- Target Windows 10/11 with DirectX 12, DXGI 1.4+ (1.5 for tearing optional).
- Prefer Root Signature v1.1 when supported; fall back to v1.0 with bounded ranges.
- Use DXC (SM6) when available; provide FXC (SM5) fallback where features permit.

Priority Guide
- P0: Blocks core functionality or causes device removal/corruption.
- P1: High risk/correctness/perf issues that affect many apps.
- P2: Important parity/perf/robustness improvements.
- P3: Diagnostics, tooling, and quality-of-life.

Validation Strategy
- Run existing test suite and sample render sessions per change set.
- Frame-diff or checksum validation for texture/buffer readbacks where applicable.
- GPU validation layer and DRED checks on debug builds to catch misuse.
- Telemetry on descriptor usage and transient allocator pressure to spot regressions.

P0 — Must Fix
- Buffer DEFAULT uploads unimplemented (DX12-001, DX12-101)
  - Symptom: Runtime updates fail for GPU-only buffers.
  - Plan: Transient UPLOAD buffer + CopyBufferRegion + barriers; reuse transient allocator.
  - Files: src/igl/d3d12/Buffer.cpp:82
  - Backlog: backlog/P0_DX12-001_Buffer_DEFAULT_upload.md

- Texture readback stubbed (DX12-002, DX12-102)
  - Symptom: copyTextureToBuffer no-op; cannot read textures.
  - Plan: CopyTextureRegion with GetCopyableFootprints; correct subresources and barriers.
  - Files: src/igl/d3d12/CommandBuffer.cpp:318
  - Backlog: backlog/P0_DX12-002_CopyTextureToBuffer.md

- Root signature ranges and feature checks (DX12-003, DX12-108)
  - Symptom: Unbounded ranges on RS v1.0/Tier-1 devices risk failure.
  - Plan: Query HighestVersion and binding tier; bound ranges to usage on fallback.
  - Files: src/igl/d3d12/Device.cpp:606-684,867-884
  - Backlog: backlog/P0_DX12-003_RootSig_Feature_Bounds.md

- Texture/sampler binding cap is 2 not 16 (DX12-004, DX12-109)
  - Symptom: Only first two texture/sampler bindings work.
  - Plan: Support IGL_TEXTURE_SAMPLERS_MAX=16; validate heap capacity; no early return.
  - Files: src/igl/d3d12/RenderCommandEncoder.cpp:640-714
  - Backlog: backlog/P0_DX12-004_Texture_Sampler_Limit.md

- Push constants mismatch and compute support (DX12-005, DX12-103, DX12-104)
  - Symptom: Graphics push constants bound as CBV; compute push constants unimplemented.
  - Plan: Bind as root 32-bit constants for graphics/compute or small dynamic CBVs consistently.
  - Files: Device.cpp (root params), Render/ComputeCommandEncoder.cpp
  - Backlog: backlog/P0_DX12-005_PushConstants_Graphics_Compute.md

- Buffer bind groups never bound (DX12-011, DX12-105)
  - Symptom: SRV/UAV/CBV buffer tables not set; features unusable.
  - Plan: Implement buffer group → root table mapping with dynamic offsets.
  - Files: src/igl/d3d12/RenderCommandEncoder.cpp:1017-1056
  - Backlog: backlog/P0_DX12-011_Buffer_Bind_Groups.md

P1 — High Priority
- Feature Level gating too strict (DX12-006, DX12-107)
  - Plan: Probe D3D12_FEATURE_FEATURE_LEVELS; use highest supported, not hard 12.0.
  - Files: src/igl/d3d12/D3D12Context.cpp:171-210
  - Backlog: backlog/P1_DX12-006_FeatureLevel_Probe.md

- Constant-buffer offset alignment handling (DX12-007, DX12-110)
  - Plan: Validate 256B alignment; provide aligned staging copy rather than silent rounding.
  - Files: src/igl/d3d12/RenderCommandEncoder.cpp:986-996,1025
  - Backlog: backlog/P1_DX12-007_CBV_Alignment.md

- Descriptor heap sizing, growth, recycling, telemetry (DX12-008, DX12-114, DX12-115)
  - Plan: Configurable sizes, high-water telemetry, free-lists/recycling and growth strategy.
  - Files: D3D12Context.cpp; DescriptorHeapManager; Render/Compute encoders.
  - Backlog: backlog/P1_DX12-008_Descriptor_Heap_Management.md

- DXGI tearing gating and flags (DX12-009, DX12-106)
  - Plan: Check DXGI_FEATURE_PRESENT_ALLOW_TEARING; create swapchain with ALLOW_TEARING flag when vsync off.
  - Files: CommandQueue.cpp; D3D12Context.cpp
  - Backlog: backlog/P1_DX12-009_Tearing_Gating.md

- Query D3D12 Options and tiers (DX12-010)
  - Plan: Cache Options/Options1+; adapt root/table usage to binding tier; inform limits.
  - Files: Device.cpp:1750-1778
  - Backlog: backlog/P1_DX12-010_D3D12_Options_Tiers.md

- Framebuffer array-layer subresource fix (DX12-111)
  - Plan: Correct D3D12CalcSubresource for array layers/cube faces in copy/resolve paths.
  - Files: src/igl/d3d12/Framebuffer.cpp:361,365
  - Backlog: backlog/P1_DX12-111_Framebuffer_ArrayLayer_Subresource.md

P2 — Important
- Cubemap uploads unimplemented (DX12-012, DX12-113)
  - Backlog: backlog/P2_DX12-012_Cubemap_Uploads.md

- Upload path efficiency and pooling (DX12-013, DX12-122)
  - Backlog: backlog/P2_DX12-013_Upload_Manager_Pooling.md

- Depth/stencil readbacks (DX12-014, DX12-112)
  - Backlog: backlog/P2_DX12-014_DSV_Readback.md

- Shader compiler fallback policy (DX12-015, DX12-116)
  - Backlog: backlog/P2_DX12-015_Shader_Compiler_Fallback.md

- Scheduling fences `waitUntilScheduled` (DX12-016, DX12-121)
  - Backlog: backlog/P2_DX12-016_WaitUntilScheduled.md

- Sampler destruction/leak (DX12-017, DX12-117)
  - Backlog: backlog/P2_DX12-017_Sampler_Destroy.md

- Device limits vs caps (DX12-018)
  - Backlog: backlog/P2_DX12-018_Device_Limits_vs_Caps.md

- bindBytes support in graphics/compute (DX12-021, DX12-022, DX12-118, DX12-119)
  - Backlog: backlog/P2_DX12-021_022_BindBytes.md

- Transient buffer cache purging (DX12-120)
  - Backlog: backlog/P2_DX12-120_Transient_Cache_Purging.md

P3 — Diagnostics
- Debug labels/markers (DX12-019)
  - Backlog: backlog/P3_DX12-019_Debug_Labels.md

- GPU timers (DX12-020, DX12-123)
  - Backlog: backlog/P3_DX12-020_GPU_Timers.md

Rollout and Risk Controls
- Land changes behind feature flags where feasible.
- Validate on Feature Levels 11_0, 11_1, 12_0, 12_1; ResourceBindingTier 1–3.
- Exercise swapchain vsync/tearing permutations and descriptor pressure stress tests.

References (starter set)
- Root signatures and feature query: https://learn.microsoft.com/windows/win32/direct3d12/root-signatures
- D3D12_FEATURE_DATA_ROOT_SIGNATURE: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_root_signature
- Uploading resources: https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources
- Resource barriers: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier
- Copyable footprints: https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints
- Descriptor heaps: https://learn.microsoft.com/windows/win32/direct3d12/descriptor-heaps
- DXGI tearing: https://learn.microsoft.com/windows/win32/direct3ddxgi/variable-refresh-rate-displays
- D3D12 options and tiers: https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
- Timestamp queries: https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_query_heap_desc
- DirectX-Graphics-Samples: https://github.com/microsoft/DirectX-Graphics-Samples

