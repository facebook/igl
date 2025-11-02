# DirectX 12 Validation Audit Report

## 1. Executive Verdict
- Overall Risk: **Fail** (Critical gaps in buffer uploads, descriptor plumbing, and push-constant handling block parity with the shipping Vulkan/OpenGL implementations).
- Severity distribution: 5 Critical / 6 High / 10 Medium / 2 Low (23 total findings).
- Module distribution: RenderCommandEncoder (6), Device & Context (6), CommandBuffer & Queue (4), Compute Encoder (3), Framebuffer (2), Texture (2).
- Full issue summary (ordered Critical → Low; recommended fix priority P0 → P3):
  1. [Critical][DX12-101] `Buffer::upload` returns `Result::Code::Unimplemented` for default-heap allocations (`src/igl/d3d12/Buffer.cpp:82`), preventing any GPU-only vertex/index/constant buffer initialization; **P0**.
  2. [Critical][DX12-102] `CommandBuffer::copyTextureToBuffer` is a stub (`src/igl/d3d12/CommandBuffer.cpp:318`), so CPU readback requests silently fail; **P0**.
  3. [Critical][DX12-103] Root parameter 0 is declared as 32-bit constants but bound via `SetGraphicsRootConstantBufferView` (`src/igl/d3d12/Device.cpp:829`, `src/igl/d3d12/RenderCommandEncoder.cpp:615`), corrupting push-constant delivery; **P0**.
  4. [Critical][DX12-104] `ComputeCommandEncoder::bindPushConstants` logs "not yet implemented" (`src/igl/d3d12/ComputeCommandEncoder.cpp:192`) while the device advertises push-constant support; **P0**.
  5. [Critical][DX12-105] Buffer bind groups are never bound (`src/igl/d3d12/RenderCommandEncoder.cpp:1084`), so descriptor-table based uniform/storage buffers are unusable; **P0**.
  6. [High][DX12-106] `DXGI_PRESENT_ALLOW_TEARING` is set without enabling `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` (`src/igl/d3d12/CommandQueue.cpp:91`, `src/igl/d3d12/D3D12Context.cpp:276`), leading to invalid `Present` calls when VSync is disabled; **P1**.
  7. [High][DX12-107] Device creation insists on `D3D_FEATURE_LEVEL_12_0` for hardware adapters (`src/igl/d3d12/D3D12Context.cpp:179`), forcing WARP on legitimate Feature Level 11 GPUs; **P1**.
  8. [High][DX12-108] Root signatures rely on unbounded descriptor ranges without checking the resource binding tier (`src/igl/d3d12/Device.cpp:597`, `829`), risking device-removal on Tier-1 hardware; **P1**.
  9. [High][DX12-109] `RenderCommandEncoder::bindTexture`/`bindSamplerState` abort after two bindings (`src/igl/d3d12/RenderCommandEncoder.cpp:632`, `695`), violating the engine-wide limit `IGL_TEXTURE_SAMPLERS_MAX = 16`; **P1**.
 10. [High][DX12-110] Constant-buffer offsets are rounded up to 256 bytes (`src/igl/d3d12/RenderCommandEncoder.cpp:1025`), misaligning client-provided offsets instead of validating them; **P1**.
 11. [High][DX12-111] `Framebuffer::copyTextureColorAttachment` ignores array layers when computing subresources (`src/igl/d3d12/Framebuffer.cpp:361`, `365`), breaking resolves for texture arrays and cube faces; **P1**.
 12. [Medium][DX12-112] Depth/stencil attachment readback helpers remain TODOs (`src/igl/d3d12/Framebuffer.cpp:300`, `307`), blocking parity with GL/Vulkan capture flows; **P2**.
 13. [Medium][DX12-113] `Texture::uploadCube` returns `Result::Code::Unimplemented` (`src/igl/d3d12/Texture.cpp:378`), preventing cubemap population; **P2**.
 14. [Medium][DX12-114] Descriptor heaps are sized to 1000/16 entries once at startup with no bounds checking or growth (`src/igl/d3d12/D3D12Context.cpp:371-392`); **P2**.
 15. [Medium][DX12-115] Descriptor indices are incremented via `nextCbvSrvUavDescriptor_++` without guarding overflow or recycling (`src/igl/d3d12/RenderCommandEncoder.cpp:708`, `src/igl/d3d12/ComputeCommandEncoder.cpp:229`); **P2**.
 16. [Medium][DX12-116] String-based shader compilation fails outright when DXC is unavailable (`src/igl/d3d12/Device.cpp:1471`), despite `getShaderVersion()` reporting an SM5 fallback; **P2**.
 17. [Medium][DX12-117] `Device::destroy(SamplerHandle)` is a stub (`src/igl/d3d12/Device.cpp:73`), leaking sampler descriptors across frames; **P2**.
 18. [Medium][DX12-118] `RenderCommandEncoder::bindBytes` is a no-op (`src/igl/d3d12/RenderCommandEncoder.cpp:568`) even though `DeviceFeatures::BindBytes` returns true; **P2**.
 19. [Medium][DX12-119] `ComputeCommandEncoder::bindBytes` is likewise unimplemented (`src/igl/d3d12/ComputeCommandEncoder.cpp:352`), blocking constant data uploads to compute shaders; **P2**.
 20. [Medium][DX12-120] `CommandBuffer::trackTransientBuffer` never purges its cache (`src/igl/d3d12/CommandBuffer.h:65-77`), accumulating per-draw allocations across submissions; **P2**.
 21. [Medium][DX12-122] `Texture::upload` recreates command allocators/lists and staging buffers per subresource (`src/igl/d3d12/Texture.cpp:286-313`), producing avoidable CPU stalls for mip/array uploads; **P2**.
 22. [Low][DX12-121] `CommandBuffer::waitUntilScheduled` is stubbed while APIs expect scheduling fences (`src/igl/d3d12/CommandBuffer.cpp:140`); **P3**.
 23. [Low][DX12-123] `Device::createTimer` returns `Result::Code::Unimplemented` (`src/igl/d3d12/Device.cpp:547`), leaving timer-based diagnostics unsupported; **P3**.

## 2. Findings Register
| ID | Severity | Category | Location | Description | Official Reference | Recommendation | Status |
|----|----------|----------|----------|-------------|--------------------|----------------|--------|
| DX12-101 | Critical | Resource Upload | `src/igl/d3d12/Buffer.cpp:82` | Default-heap buffer uploads return `Result::Code::Unimplemented`, so GPU-only buffers cannot be initialized. | Doc: [Uploading resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources#uploading-to-the-default-heap); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp | Implement `UpdateSubresources` / copy via a staging allocator before returning success. | Open |
| DX12-102 | Critical | Readback | `src/igl/d3d12/CommandBuffer.cpp:318` | `CommandBuffer::copyTextureToBuffer` is a stub, so CPU readback requests silently drop. | Doc: [Texture copy operations](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp | Allocate readback resources and issue `CopyTextureRegion` plus fence wait. | Open |
| DX12-103 | Critical | Root Signature | `src/igl/d3d12/Device.cpp:829; src/igl/d3d12/RenderCommandEncoder.cpp:615` | Root parameter 0 is defined as 32-bit constants but bound as a CBV, corrupting push-constant data. | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/CommandContext.cpp | Switch parameter 0 to `SetGraphicsRoot32BitConstants` or change the root signature to a CBV and bind a transient buffer once. | Open |
| DX12-104 | Critical | Compute | `src/igl/d3d12/ComputeCommandEncoder.cpp:192` | Compute push constants are logged as "not yet implemented" even though the feature is advertised. | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp | Implement compute push-constant uploads via `SetComputeRoot32BitConstants` / transient CBVs. | Open |
| DX12-105 | Critical | Descriptors | `src/igl/d3d12/RenderCommandEncoder.cpp:1084` | Buffer bind groups are ignored, so descriptor-table uniform/storage buffers never reach shaders. | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#descriptor-tables); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp | Allocate CBV/UAV descriptors per bind group and point root parameters 1–3 at the populated tables. | Open |
| DX12-106 | High | Swapchain | `src/igl/d3d12/CommandQueue.cpp:91; src/igl/d3d12/D3D12Context.cpp:276` | Presents request tearing without enabling `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`, violating DXGI requirements. | Doc: [Variable-refresh-rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays#enabling-tearing); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloFrameBuffering/D3D12HelloFrameBuffering.cpp | Gate tearing via `CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING)` and set swap-chain flags before Present. | Open |
| DX12-107 | High | Initialization | `src/igl/d3d12/D3D12Context.cpp:179` | Hardware device creation insists on Feature Level 12_0, falling back to WARP for FL11 hardware. | Doc: [D3D12 feature level](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-feature-level); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTriangle/D3D12HelloTriangle.cpp | Attempt creation at FL11_0, probe for the highest supported level, and only fall back to WARP if all hardware fails. | Open |
| DX12-108 | High | Root Signature | `src/igl/d3d12/Device.cpp:597; src/igl/d3d12/Device.cpp:829` | Unbounded descriptor ranges are used without checking `D3D12_FEATURE_DATA_D3D12_OPTIONS`, which Tier-1 hardware rejects. | Doc: [Hardware resource binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-resource-binding#resource-binding-tiers); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/RootSignature.cpp | Query the binding tier and clamp descriptor counts or build fallback root signatures for Tier-1 devices. | Open |
| DX12-109 | High | Descriptor Binding | `src/igl/d3d12/RenderCommandEncoder.cpp:632; src/igl/d3d12/RenderCommandEncoder.cpp:695` | Texture/sampler binding stops after two slots, contradicting `IGL_TEXTURE_SAMPLERS_MAX = 16`. | Doc: [Hardware resource binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-resource-binding#descriptor-heaps-and-tables); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Graphics/CommandContext.cpp | Allow indices up to 16 and advance descriptor handles using the heap manager to keep tables contiguous. | Open |
| DX12-110 | High | Resource Binding | `src/igl/d3d12/RenderCommandEncoder.cpp:1025` | Constant-buffer offsets are rounded up to 256 bytes instead of validated, shifting user-provided data. | Doc: [Constant buffer and resource binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/constant-buffer-and-resource-binding#common-constant-buffer-scenarios); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/DynamicUploadHeap.cpp | Reject unaligned offsets (or copy into an aligned scratch buffer) instead of silently bumping the pointer. | Open |
| DX12-111 | High | Framebuffer | `src/igl/d3d12/Framebuffer.cpp:361; src/igl/d3d12/Framebuffer.cpp:365` | Copying color attachments ignores array layers when computing subresource indices. | Doc: [Resource subresources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-subresources); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp | Use `Texture::calcSubresourceIndex` for both source and destination before issuing `CopyTextureRegion`. | Open |
| DX12-112 | Medium | Readback | `src/igl/d3d12/Framebuffer.cpp:300; src/igl/d3d12/Framebuffer.cpp:307` | Depth/stencil readback helpers are TODOs, preventing capture of depth buffers. | Doc: [Texture copy operations](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloDepthStencil/D3D12HelloDepthStencil.cpp | Implement typeless depth copies into staging resources and swizzle to CPU-readable formats. | Open |
| DX12-113 | Medium | Texture | `src/igl/d3d12/Texture.cpp:378` | Cubemap upload is unimplemented. | Doc: [Cube-map texture](https://learn.microsoft.com/en-us/windows/win32/direct3d12/cube-map-texture); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloCube/D3D12HelloCube.cpp | Mirror the Vulkan path: loop faces/mips and stage data via `UpdateSubresources`. | Open |
| DX12-114 | Medium | Scalability | `src/igl/d3d12/D3D12Context.cpp:371-392` | Descriptor heaps are hard-sized (1000/16) with no scaling or overflow handling. | Doc: [Descriptor heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/DescriptorHeap.cpp | Expose heap sizing knobs and detect exhaustion to allocate additional shader-visible heaps or compact descriptors. | Open |
| DX12-115 | Medium | Descriptors | `src/igl/d3d12/RenderCommandEncoder.cpp:708; src/igl/d3d12/ComputeCommandEncoder.cpp:229` | Descriptor indices advance with `nextCbvSrvUavDescriptor_++` ignoring heap bounds and free lists. | Doc: [Descriptor heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/DescriptorHeap.cpp | Route allocations through `DescriptorHeapManager::allocate*` and recycle indices once GPU work completes. | Open |
| DX12-116 | Medium | Shader Toolchain | `src/igl/d3d12/Device.cpp:1471` | If DXC is unavailable, shader compilation fails instead of falling back to FXC/SPIR-V. | Doc: [Using shader compilers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-using-shader-compilers); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTriangle/D3D12HelloTriangle.cpp | Detect DXC absence and fall back to `D3DCompile` or require precompiled DXIL. | Open |
| DX12-117 | Medium | Resource Lifetime | `src/igl/d3d12/Device.cpp:73` | `Device::destroy(SamplerHandle)` is empty, leaking sampler descriptors. | Doc: [Descriptor heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Graphics/SamplerManager.cpp | Return sampler indices to the descriptor heap manager during destruction. | Open |
| DX12-118 | Medium | API Compliance | `src/igl/d3d12/RenderCommandEncoder.cpp:568` | `bindBytes` is a no-op despite the feature flag reporting support. | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/CommandContext.cpp | Upload into a transient constant buffer or emit `SetGraphicsRoot32BitConstants`. | Open |
| DX12-119 | Medium | API Compliance | `src/igl/d3d12/ComputeCommandEncoder.cpp:352` | Compute `bindBytes` is unimplemented. | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp | Provide compute-side transient constant uploads or mark the feature unsupported. | Open |
| DX12-120 | Medium | Memory Lifetime | `src/igl/d3d12/CommandBuffer.h:65-77` | `trackTransientBuffer` grows without ever clearing, holding buffers past GPU completion. | Doc: [Memory management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/DynamicUploadHeap.cpp | Clear tracked buffers after fence completion or use a ring allocator. | Open |
| DX12-121 | Low | Synchronization | `src/igl/d3d12/CommandBuffer.cpp:140` | `waitUntilScheduled` is stubbed, so CPU synchronization APIs lie about scheduling status. | Doc: [Using fences](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-fences); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloFrameBuffering/D3D12HelloFrameBuffering.cpp | Hook scheduling fences or clearly report the feature as unsupported. | Open |
| DX12-122 | Medium | Performance | `src/igl/d3d12/Texture.cpp:286-313` | Each subresource upload allocates fresh command allocators/lists and staging buffers, inflating CPU cost. | Doc: [Uploading resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources#uploading-to-the-default-heap); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp | Reuse a single upload command list per submission and batch `CopyTextureRegion` calls. | Open |
| DX12-123 | Low | Instrumentation | `src/igl/d3d12/Device.cpp:547` | `Device::createTimer` returns `Result::Code::Unimplemented`, disabling GPU timing. | Doc: [GPU timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/gpu-timing); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/GpuTimer.cpp | Implement timestamp queries (begin/end, resolve, fence) or gate the feature flag. | Open |

## 3. Detailed Analyses (Per Analyst)

### Analyst-A API & Initialization
- **DX12-106 (High)**: The present path sets `DXGI_PRESENT_ALLOW_TEARING` without enabling `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`, contradicting [Microsoft's tearing guidance](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays#enabling-tearing) and the pattern in `microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloFrameBuffering/D3D12HelloFrameBuffering.cpp`.
- **DX12-107 (High)**: `createDevice` only accepts Feature Level 12_0 before falling back to WARP, unlike `microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTriangle/D3D12HelloTriangle.cpp` which starts at FL11_0 per [feature-level guidance](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-feature-level).
- **DX12-114 (Medium)**: Descriptor heap capacities are hard-coded (1000/16) with no scaling or overflow handling, whereas `MiniEngine/Core/DescriptorHeap.cpp` resizes based on workload.
- **DX12-123 (Low)**: GPU timer creation is unimplemented, preventing the DX12 backend from matching the Vulkan/GL timing APIs described in [GPU timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/gpu-timing).

### Analyst-B Resources & Memory Lifetime
- **DX12-101 (Critical)** and **DX12-102 (Critical)** block default-heap population and texture readback, violating [Uploading resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources#uploading-to-the-default-heap) and [Texture copy operations](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations).
- **DX12-110 (High)** misaligns constant-buffer offsets instead of validating 256-byte constraints, risking shader crashes relative to `MiniEngine/Core/DynamicUploadHeap.cpp`.
- **DX12-111 (High)**, **DX12-112 (Medium)**, and **DX12-113 (Medium)** leave array-layer copies, depth/stencil readback, and cubemap uploads incomplete, unlike the cross-platform Vulkan/OpenGL implementations.
- **DX12-120 (Medium)** leaks transient buffers across submissions; Vulkan uses ring buffers (`src/igl/vulkan/VulkanContext.cpp:1321`) to recycle allocations.
- **DX12-122 (Medium)** recreates upload command lists per subresource, a known anti-pattern compared with the batched approach in `D3D12HelloTexture`.

### Analyst-C Descriptors & Root Signatures
- **DX12-103 (Critical)**, **DX12-104 (Critical)**, and **DX12-105 (Critical)** show incomplete root-signature usage and bind-group plumbing versus the patterns in `MiniEngine/Core/CommandContext.cpp` and `D3D12nBodyGravity`.
- **DX12-108 (High)** fails to validate binding tiers before using unbounded descriptor ranges, contradicting [Hardware resource binding tiers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-resource-binding#resource-binding-tiers).
- **DX12-109 (High)** and **DX12-115 (Medium)** cap texture/sampler bindings at two slots and bypass the descriptor-heap manager, whereas Vulkan (`src/igl/vulkan/ResourcesBinder.cpp:73`) supports the full 16-unit contract.
- **DX12-117 (Medium)**, **DX12-118 (Medium)**, and **DX12-119 (Medium)** leave sampler destruction and `bindBytes` APIs as stubs, misrepresenting feature coverage.

### Analyst-D Command Queues & Synchronization
- **DX12-106 (High)** surfaces in queue submission, and **DX12-121 (Low)** leaves `waitUntilScheduled` unimplemented, making CPU-facing timeline APIs unreliable relative to [Using fences](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-fences).
- **DX12-120 (Medium)** stores transient buffers indefinitely, so queue fences must be integrated to release memory when GPU work completes.

### Analyst-E Shaders & Toolchain
- **DX12-103 (Critical)** and **DX12-104 (Critical)** expose mismatched root-constant plumbing for graphics and compute shaders.
- **DX12-116 (Medium)** depends solely on DXC despite [Using shader compilers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-using-shader-compilers) recommending an FXC fallback, unlike `D3D12HelloTriangle`.
- **DX12-118 (Medium)** and **DX12-119 (Medium)** leave `bindBytes` unimplemented, so HLSL roots expecting push constants receive zeroed data.

### Analyst-F Ray Tracing (DXR)
- No DXR entry points, pipeline-state builders, or shader-library exports exist in the D3D12 backend. If DXR parity is required, the entire flow from [`directx-raytracing-dxr`](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-raytracing-dxr) and `microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12Raytracing/` must be planned; currently DXR is effectively unsupported.

### Analyst-G Performance & Scalability
- **DX12-114 (Medium)** and **DX12-115 (Medium)** threaten descriptor exhaustion on content-rich scenes, unlike the pool expansion logic in `MiniEngine`.
- **DX12-120 (Medium)** and **DX12-122 (Medium)** add per-frame CPU overhead and memory growth; Vulkan amortizes these costs via persistent command allocators (`src/igl/vulkan/VulkanContext.cpp:2272`).
- Buffer upload/readback gaps (**DX12-101/102**) also prevent staging optimizations common in the DirectX samples.

### Analyst-H Robustness & Error Handling
- Critical resource operations (**DX12-101**, **DX12-102**) bail out silently, and depth/stencil helpers (**DX12-112**) remain TODOs, so diagnostics and capture tooling fail.
- Descriptor cleanup (**DX12-117**) and transient-buffer lifetime (**DX12-120**) leak resources across submissions.
- Synchronization hooks (**DX12-121**) do not honor the API contract exposed to higher layers.

### Analyst-I Hard-coded Limits & Cross-API Validation
- Texture/sampler binding (**DX12-109**) caps at two entries despite `IGL_TEXTURE_SAMPLERS_MAX = 16` (`src/igl/Common.h:30`) and Vulkan enforcing the limit (`src/igl/vulkan/ResourcesBinder.cpp:73`).
- Descriptor heap capacities (**DX12-114**, **DX12-115**) ignore hardware limits queried on the Vulkan backend (`src/igl/vulkan/Device.cpp:723`).
- Resource readback gaps (**DX12-111**, **DX12-112**, **DX12-113**) break feature parity with the Vulkan and OpenGL implementations that cover the same scenarios.

## 4. Cross-API Limit Audit
| Constraint | D3D12 Observation | Vulkan / OpenGL Reference | Status | Notes |
|------------|-------------------|---------------------------|--------|-------|
| Fragment textures & samplers per pass | `bindTexture` / `bindSamplerState` early-return when `index >= 2` (`src/igl/d3d12/RenderCommandEncoder.cpp:632`, `695`). | `IGL_TEXTURE_SAMPLERS_MAX = 16` (`src/igl/Common.h:30`); Vulkan enforces the full range (`src/igl/vulkan/ResourcesBinder.cpp:73`, `91`). | **No** | Violates engine contract; scene graphs using >2 bindings fail on D3D12. |
| Shader-visible CBV/SRV/UAV heap capacity | Fixed to 1000 descriptors with no guard (`src/igl/d3d12/D3D12Context.cpp:371-379`). | Vulkan sizes descriptor pools from `VkPhysicalDeviceLimits` (`src/igl/vulkan/Device.cpp:723`). | **Partial** | Works for simple demos but will overflow on texture-heavy content. |
| Push constants (graphics & compute) | Graphics mis-binds root constants; compute path is stubbed (`src/igl/d3d12/Device.cpp:829`, `src/igl/d3d12/ComputeCommandEncoder.cpp:192`). | Vulkan uses `vkCmdPushConstants` for both pipelines (`src/igl/vulkan/RenderCommandEncoder.cpp:518`, `src/igl/vulkan/ComputeCommandEncoder.cpp:271`). | **No** | D3D12 cannot ingest push constants while Vulkan/GL rely on them. |
| BindGroup buffer descriptors | Buffer bind groups log "not yet implemented" (`src/igl/d3d12/RenderCommandEncoder.cpp:1084`). | Vulkan binds buffer groups through descriptor sets (`src/igl/vulkan/RenderCommandEncoder.cpp:996`). | **No** | Breaks cross-platform uniform/storage buffer binding APIs. |

## 5. Compliance Matrix vs Official References
| Topic | Status | Notes | References |
|-------|--------|-------|------------|
| Device creation fallback to FL11 | **No** | DX12-107 requests FL12_0 only, unlike `D3D12HelloTriangle`. | [Feature level](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-feature-level); microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTriangle/D3D12HelloTriangle.cpp |
| Swapchain tearing enablement | **No** | DX12-106 calls `Present` with `ALLOW_TEARING` without setting swap-chain flags. | [Variable-refresh-rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays#enabling-tearing); microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloFrameBuffering/D3D12HelloFrameBuffering.cpp |
| Default-heap upload support | **No** | DX12-101 drops uploads, contrary to [Uploading resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources#uploading-to-the-default-heap). | microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp |
| Texture readback (color/depth/stencil) | **Partial** | Color path exists but `copyTextureToBuffer`, depth, and stencil helpers (DX12-102/112) are stubs. | [Texture copy operations](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations); microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp; microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloDepthStencil/D3D12HelloDepthStencil.cpp |
| Push constants (graphics & compute) | **No** | Graphics mis-binds root constants and compute is unimplemented (DX12-103/104). | [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); microsoft/DirectX-Graphics-Samples/MiniEngine/Core/CommandContext.cpp |
| Descriptor binding parity with Vulkan/GL | **No** | Texture/sampler slots capped at 2 and descriptor heaps lack overflow handling (DX12-109/114/115). | [Hardware resource binding](https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-resource-binding#descriptor-heaps-and-tables); `src/igl/vulkan/ResourcesBinder.cpp` |
| BindGroup buffer API | **No** | DX12-105 leaves buffer groups unbound compared with Vulkan's descriptor sets. | [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#descriptor-tables); microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp |
| Shader compiler fallback | **No** | DX12-116 fails when DXC is absent; official guidance recommends FXC fallback. | [Using shader compilers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-using-shader-compilers); microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTriangle/D3D12HelloTriangle.cpp |
| DirectX Raytracing support | **No** | No DXR interfaces implemented in DX12 backend; Vulkan/GL expose ray-tracing alternatives separately. | [DXR overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-raytracing-dxr); microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12Raytracing/ |
| PIX / ETW instrumentation | **Partial** | Compute encoders emit PIX markers (`src/igl/d3d12/ComputeCommandEncoder.cpp:432-454`), but render encoders stub out debug groups (`src/igl/d3d12/RenderCommandEncoder.cpp:926`). | [PIX debugging](https://learn.microsoft.com/en-us/gaming/tools/PIX/); microsoft/DirectX-Graphics-Samples/MiniEngine/Core/CommandContext.cpp |

## 6. Unimplemented & Stubbed Functions Audit
| ID | Severity | Symbol | Location | Stub Pattern | Callers Affected | Runtime Impact | Microsoft Ref | Suggested Impl / Plan | Owner | ETA |
|----|----------|--------|----------|--------------|------------------|----------------|----------------|------------------------|-------|-----|
| DX12-101 | Critical | `Buffer::upload` (DEFAULT path) | `src/igl/d3d12/Buffer.cpp:82` | Returns `Result::Code::Unimplemented` | `Device::createBuffer`, transient constant updates | GPU-only vertex/index/CB buffers cannot be populated | Doc: [Uploading resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources#uploading-to-the-default-heap); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp | Implement staging via `UpdateSubresources` and keep the upload buffer alive until the submission fence signals. | DX12 Backend | TBD |
| DX12-102 | Critical | `CommandBuffer::copyTextureToBuffer` | `src/igl/d3d12/CommandBuffer.cpp:318` | Empty body | Frame capture, screenshot tooling | Color readback silently fails | Doc: [Texture copy operations](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp | Allocate readback resources and issue `CopyTextureRegion` plus fence wait. | DX12 Backend | TBD |
| DX12-104 | Critical | `ComputeCommandEncoder::bindPushConstants` | `src/igl/d3d12/ComputeCommandEncoder.cpp:192` | Logs "not yet implemented" | Compute pipelines expecting push constants | Compute shaders read junk push-constant data | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp | Upload data via `SetComputeRoot32BitConstants` or a transient CBV bound to the push-constant slot. | DX12 Backend | TBD |
| DX12-105 | Critical | `RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle,...)` | `src/igl/d3d12/RenderCommandEncoder.cpp:1084` | Logs "Not yet implemented" | All buffer bind groups | Uniform/storage buffers never reach shaders | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#descriptor-tables); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp | Allocate CBV/UAV descriptors per bind group and point root descriptor tables before draw/dispatch. | DX12 Backend | TBD |
| DX12-112 | Medium | `Framebuffer::copyBytesDepthAttachment` / `copyBytesStencilAttachment` | `src/igl/d3d12/Framebuffer.cpp:300-307` | TODO comment | Debug capture, depth validation tests | Depth/stencil readback unsupported | Doc: [Texture copy operations](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloDepthStencil/D3D12HelloDepthStencil.cpp | Create typeless depth resources and copy plane data into staging buffers before CPU access. | DX12 Backend | TBD |
| DX12-113 | Medium | `Texture::uploadCube` | `src/igl/d3d12/Texture.cpp:378` | Returns `Result::Code::Unimplemented` | Cube-map textures, skyboxes | Cubemap assets cannot load on DX12 | Doc: [Cube-map texture](https://learn.microsoft.com/en-us/windows/win32/direct3d12/cube-map-texture); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloWorld/src/HelloCube/D3D12HelloCube.cpp | Loop faces/mips and reuse the staging path created for 2D textures. | DX12 Backend | TBD |
| DX12-117 | Medium | `Device::destroy(SamplerHandle)` | `src/igl/d3d12/Device.cpp:73` | Empty body | Sampler lifetime management | Sampler descriptors leak until shutdown | Doc: [Descriptor heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Graphics/SamplerManager.cpp | Return sampler indices to the descriptor heap manager and recycle them per frame. | DX12 Backend | TBD |
| DX12-118 | Medium | `RenderCommandEncoder::bindBytes` | `src/igl/d3d12/RenderCommandEncoder.cpp:568` | Empty method | High-level `bindBytes` feature | Render shaders expecting inline bytes receive zeroed data | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/CommandContext.cpp | Upload into a transient constant buffer or emit `SetGraphicsRoot32BitConstants`. | DX12 Backend | TBD |
| DX12-119 | Medium | `ComputeCommandEncoder::bindBytes` | `src/igl/d3d12/ComputeCommandEncoder.cpp:352` | Logs "not yet implemented" | Compute shaders using `bindBytes` | Compute constants unavailable | Doc: [Using root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#setting-root-constants); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12nBodyGravity/D3D12nBodyGravity.cpp | Match Vulkan by emitting compute root constants or temporary CBVs. | DX12 Backend | TBD |
| DX12-121 | Low | `CommandBuffer::waitUntilScheduled` | `src/igl/d3d12/CommandBuffer.cpp:140` | Stubbed out | CPU synchronization helpers | API consumers misreport scheduling status | Doc: [Using fences](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-fences); Sample: microsoft/DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloFrameBuffering/D3D12HelloFrameBuffering.cpp | Signal a fence when enqueuing the command buffer and wait on it here, or document the limitation. | DX12 Backend | TBD |
| DX12-123 | Low | `Device::createTimer` | `src/igl/d3d12/Device.cpp:547` | Returns `Result::Code::Unimplemented` | Performance instrumentation, frame captures | GPU timing unavailable on DX12 | Doc: [GPU timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/gpu-timing); Sample: microsoft/DirectX-Graphics-Samples/MiniEngine/Core/GpuTimer.cpp | Implement timestamp-heap based timers (begin/end, resolve, fence) or mark the feature unsupported. | DX12 Backend | TBD |
## 7. Appendices

### A. Queue / Fence Timeline
```text
CommandQueue::submit (src/igl/d3d12/CommandQueue.cpp:60-133)
  ├─ Ensure command list closed
  ├─ ExecuteCommandLists
  ├─ Signal fence[++fenceValue] on the graphics queue
  ├─ Present (optionally with tearing)
  └─ Advance currentFrameIndex = (currentFrameIndex + 1) % 3
Next-frame reuse:
  Wait on frameContexts[nextFrameIndex].fenceValue before resetting per-frame resources.
Note: currentFrameIndex is advanced manually instead of querying swapChain->GetCurrentBackBufferIndex(), risking desynchronisation.
```

### B. Resource-State Transition Map
- `RenderCommandEncoder::bindTexture` transitions sampled resources to `D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE` for all subresources (`src/igl/d3d12/RenderCommandEncoder.cpp:733-758`).
- `RenderCommandEncoder::endEncoding` restores attachments to shader-readable or present states (`src/igl/d3d12/RenderCommandEncoder.cpp:430-466`).
- `Texture::upload` transitions the target subresource to `COPY_DEST` before `CopyTextureRegion` and back to its prior state (`src/igl/d3d12/Texture.cpp:300-312`).
- Missing transitions: depth/stencil readback helpers and buffer bind-group stubs never pivot states, leaving copy paths incomplete.

### C. Descriptor-Heap Utilization Snapshot
| Heap | Configured Count | Source | Notes |
|------|------------------|--------|-------|
| CBV/SRV/UAV (shader-visible) | 1000 | `src/igl/d3d12/D3D12Context.cpp:371-379` | Hard cap shared by textures, buffers, UAVs; no overflow handling. |
| Sampler (shader-visible) | 16 | `src/igl/d3d12/D3D12Context.cpp:382-391` | Matches `IGL_TEXTURE_SAMPLERS_MAX`, but binding code only exposes two slots. |
| RTV (CPU-visible) | 64 | `src/igl/d3d12/DescriptorHeapManager.cpp:45-63` | Allocated via manager; freed at endEncoding. |
| DSV (CPU-visible) | 32 | `src/igl/d3d12/DescriptorHeapManager.cpp:65-83` | Depth/stencil copy TODOs mean DSVs are rarely reclaimed. |

### D. DXC Compile-Flag Matrix
| Mode | Flags Passed | Source | Notes |
|------|--------------|--------|-------|
| Debug builds | `D3DCOMPILE_DEBUG`, `D3DCOMPILE_SKIP_OPTIMIZATION`, `-Zi`, `-Qembed_debug`, `-Od` | `src/igl/d3d12/Device.cpp:1417-1450`; `src/igl/d3d12/DXCCompiler.cpp:60-152` | Emits unsigned DXIL; relies on `D3D12EnableExperimentalFeatures` if validator unavailable. |
| Release builds | `-O3` (unless `IGL_D3D12_DISABLE_SHADER_DEBUG=1`), optional `-WX` | `src/igl/d3d12/Device.cpp:1435-1450`; `src/igl/d3d12/DXCCompiler.cpp:93-138` | No FXC fallback when DXC is missing (DX12-116). |

### E. PIX / ETW Profiling Notes
- Render encoders leave `pushDebugGroupLabel`, `insertDebugEventLabel`, and `popDebugGroupLabel` empty (`src/igl/d3d12/RenderCommandEncoder.cpp:926-934`), so PIX captures lack draw-level markers.
- Compute encoders emit proper PIX markers via `BeginEvent`/`SetMarker`/`EndEvent` (`src/igl/d3d12/ComputeCommandEncoder.cpp:432-454`).
- No ETW or PIX capture automation scripts are present under `tools/` for the D3D12 backend.

### F. Unimplemented Coverage Map
```text
Critical (P0)
  - DX12-101 Buffer::upload ➜ blocks GPU-only buffers
  - DX12-102 CommandBuffer::copyTextureToBuffer ➜ prevents readback
  - DX12-103 Render push constants ➜ root constants mis-bound
  - DX12-104 Compute push constants ➜ feature advertised but absent
  - DX12-105 BindGroup buffer binding ➜ descriptor tables unused
High (P1)
  - DX12-106 Swapchain tearing flags
  - DX12-107 Hardware feature-level fallback
  - DX12-108 Unbounded descriptor ranges
  - DX12-109 Texture/sampler binding cap
  - DX12-110 Constant-buffer offset handling
  - DX12-111 Array-layer copy logic
Medium (P2)
  - DX12-112 Depth/stencil readback
  - DX12-113 Cubemap upload
  - DX12-114/115 Descriptor heap sizing & allocation
  - DX12-116 Shader compiler fallback
  - DX12-117 Sampler destruction
  - DX12-118/119 bindBytes support
  - DX12-120 Transient buffer lifetime
  - DX12-122 Texture upload batching
Low (P3)
  - DX12-121 waitUntilScheduled stub
  - DX12-123 Timer creation stub
Dependencies
  - Implementing DX12-101, DX12-102, DX12-105 unlocks most resource + encoder features.
  - DX12-108, DX12-109, DX12-115 must land before we can mirror Vulkan descriptor binding behaviour.
```
