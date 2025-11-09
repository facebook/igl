# DirectX 12 Audit Report – Codex

## 1. Executive Verdict
- Overall risk: **Fail (Critical)** – The D3D12 backend violates Microsoft resource-binding requirements, can overflow descriptor heaps (especially in headless mode), and leaves core compute/descriptor features unbound, so the backend is not conformant.
- Critical (2):
  - **DX12-FIND-01** – Render root signatures always declare `UINT_MAX` SRV/sampler ranges even on ResourceBindingTier‑1 hardware (src/igl/d3d12/Device.cpp:956-965), which fails device creation per Microsoft limits.
  - **DX12-FIND-02** – Descriptor heaps are linear allocators with no bounds checking, and headless submissions never reset `nextCbvSrvUavDescriptor`/`nextSamplerDescriptor` (src/igl/d3d12/RenderCommandEncoder.cpp:869-884; src/igl/d3d12/CommandQueue.cpp:212-323), so >1024 SRVs or long-running headless sessions write past heap memory and cause device removal.
- High (4):
  - **DX12-FIND-03** – `CommandQueue::submit` signals `scheduleFence_` with value `1` every time (src/igl/d3d12/CommandQueue.cpp:142-144) even though `ID3D12CommandQueue::Signal` must be monotonic, so `CommandBuffer::waitUntilScheduled()` silently fails for every submission after the first.
  - **DX12-FIND-04** – Compute CBVs (root parameter 3) are never populated; the encoder just logs “using direct root CBV for now” (src/igl/d3d12/ComputeCommandEncoder.cpp:155-170), so compute constant buffers are undefined.
  - **DX12-FIND-05** – Bind-group storage buffers are logged as “pending RS update” and never bound into SRV/UAV tables (src/igl/d3d12/RenderCommandEncoder.cpp:1518-1531), so descriptor-based storage buffers cannot be used.
  - **DX12-FIND-06** – `Texture::upload` swaps R/B bytes for `RGBA_UNorm8/SRGB` even though DXGI stores RGBA directly (src/igl/d3d12/Texture.cpp:13-25,213-242), corrupting all RGBA textures.
- Medium (6):
  - **DX12-FIND-07** – Every buffer upload calls `ctx_->waitForGPU()` (src/igl/d3d12/Device.cpp:312-365), stalling the entire queue per resource and preventing overlap.
  - **DX12-FIND-08** – Sampler heaps are hard-coded to 32 descriptors per frame (src/igl/d3d12/Common.h:33; src/igl/d3d12/D3D12Context.cpp:528-541) while Vulkan dynamically grows to device limits (src/igl/vulkan/VulkanContext.cpp:1125-1170), creating portability gaps.
  - **DX12-FIND-09** – Push constants are capped at 16 DWORDs/64 bytes for both compute and graphics (src/igl/d3d12/Device.cpp:736-747,932-943) whereas Vulkan exposes 128 bytes by default; D3D12 allows 64 DWORDs, so the backend underutilizes available space.
  - **DX12-FIND-10** – Depth and stencil readback helpers are TODOs (src/igl/d3d12/Framebuffer.cpp:340-347), blocking renderpass readbacks.
  - **DX12-FIND-11** – `Device::createTimer` always returns `Unimplemented`, so GPU timestamp queries are unavailable (src/igl/d3d12/Device.cpp:657).
  - **DX12-FIND-12** – `Texture::uploadCube` is a stub returning `Unimplemented` (src/igl/d3d12/Texture.cpp:335-340).
- Low (1):
  - **DX12-FIND-13** – `multiDrawIndirect` and `multiDrawIndexedIndirect` are empty stubs (src/igl/d3d12/RenderCommandEncoder.cpp:1158-1163), so the API surface is incomplete.
- Totals: 13 findings (Descriptors & Root Signatures: 5; Command/Synchronization: 2; Resources & Memory: 3; Feature/Tooling gaps: 3).

## 2. Findings Register
| ID | Severity | Category | Location | Description | Official Reference | Recommendation | Status |
|----|----------|----------|----------|-------------|--------------------|----------------|--------|
| DX12-FIND-01 | Critical | Root Signature | `src/igl/d3d12/Device.cpp:956-965` | Render root signatures use `UINT_MAX` descriptor counts for SRVs/samplers regardless of `resourceBindingTier_`, which violates Tier‑1 requirements and causes device creation failure on Tier‑1 adapters. | [Resource Binding – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding) | Clamp descriptor range sizes to the Tier reported by `CheckFeatureSupport`, emit RS 1.0 fallback for Tier‑1, and validate during device init. | Open |
| DX12-FIND-02 | Critical | Descriptors | `src/igl/d3d12/RenderCommandEncoder.cpp:869-884`, `src/igl/d3d12/CommandQueue.cpp:212-323`, `src/igl/d3d12/HeadlessContext.cpp:107-190` | Per-frame descriptor heaps are simple counters with no bounds checks; headless submissions never reset the counters, so long sessions or >1024 SRVs overflow shader-visible heaps and corrupt memory. | [Descriptor Heaps – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)<br>[MiniEngine DynamicDescriptorHeap](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/DynamicDescriptorHeap.cpp) | Track heap capacity, grow or spill descriptors when near exhaustion, and always reset counters when advancing frames (even when no swapchain). | Open |
| DX12-FIND-03 | High | Synchronization | `src/igl/d3d12/CommandQueue.cpp:142-144` | `scheduleFence_` is always signaled with value `1`, violating the monotonic fence rule and making `CommandBuffer::waitUntilScheduled()` a no-op after the first submission. | [ID3D12CommandQueue::Signal – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal) | Track a per-command-buffer fence counter and increment per submission before calling `Signal`, mirroring Microsoft samples. | Open |
| DX12-FIND-04 | High | Compute Binding | `src/igl/d3d12/ComputeCommandEncoder.cpp:155-170` | Compute root parameter 3 (CBV table) is never populated; the code logs “CBV table binding … using direct root CBV for now” and proceeds without binding descriptors, so compute constant buffers read garbage. | [Using Root Signatures – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-root-signatures#descriptor-tables) | Allocate CBV descriptors in the per-frame heap, write them with `CreateConstantBufferView`, and bind the GPU handle via `SetComputeRootDescriptorTable(3, …)`. | Open |
| DX12-FIND-05 | High | Descriptors | `src/igl/d3d12/RenderCommandEncoder.cpp:1518-1531` | `bindBindGroup` ignores storage buffers, logging “SRV/UAV table binding pending” instead of creating UAV/SRV descriptors, so bind-group storage buffers are unusable. | [Resource Binding – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding) | Map bind-group storage slots to SRV/UAV descriptor ranges and emit the descriptors (mirroring Vulkan ResourcesBinder) before draw/dispatch. | Open |
| DX12-FIND-06 | High | Texture Upload | `src/igl/d3d12/Texture.cpp:13-25,213-242` | `needsRGBAChannelSwap` swaps R/B bytes for `RGBA_UNorm8/SRGB`, yet textures are created as `DXGI_FORMAT_R8G8B8A8_*`, so every upload turns RGBA data into BGRA. | [DXGI Formats – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-format) | Remove the swap for RGBA formats (only swap when the DXGI format is BGRA) or create BGRA textures when the data is BGRA. | Open |
| DX12-FIND-07 | Medium | Performance | `src/igl/d3d12/Device.cpp:312-365` | DEFAULT-heap buffer uploads call `ctx_->waitForGPU()` after each copy, forcing a full queue flush per resource. | [Uploading Resources – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources) | Use per-upload fences or frame fences to retire staging buffers without blocking; mirror HelloTriangle/UpdateSubresources pattern. | Open |
| DX12-FIND-08 | Medium | Limits | `src/igl/d3d12/Common.h:33`, `src/igl/d3d12/D3D12Context.cpp:528-541`, `src/igl/vulkan/VulkanContext.cpp:1125-1170` | Sampler heaps are capped at 32 descriptors regardless of `D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE` (2048) while Vulkan dynamically grows to the hardware limit, so complex scenes cannot bind more than 32 samplers in DX12. | [Descriptor Heaps – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps) | Query `D3D12_FEATURE_DATA_D3D12_OPTIONS` / hardware limits and grow sampler heaps or implement sampler heap pooling similar to MiniEngine. | Open |
| DX12-FIND-09 | Medium | Cross-API Limits | `src/igl/d3d12/Device.cpp:736-747,932-943`, `src/igl/vulkan/PipelineState.cpp:55-70` | Push constants/root constants are limited to 16 DWORDs (64 bytes) although D3D12 allows 64 DWORDs (256 bytes) and the Vulkan backend exposes 128 bytes, breaking parity for shader interfaces. | [D3D12_ROOT_CONSTANTS – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_constants)<br>[VkPhysicalDeviceLimits – Khronos](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html) | Expose at least the 128 byte budget shared across backends (clamp to 64 DWORD cap) and validate requested ranges per shader. | Open |
| DX12-FIND-10 | Medium | Framebuffer | `src/igl/d3d12/Framebuffer.cpp:340-347` | `copyBytesDepthAttachment` / `copyBytesStencilAttachment` are TODOs, so depth/stencil readbacks from framebuffer attachments cannot work. | [Texture Copy Operations – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations) | Implement the readback path using `CopyTextureRegion` into a readback heap and mirror the color-attachment cache logic. | Open |
| DX12-FIND-11 | Medium | Profiling | `src/igl/d3d12/Device.cpp:657` | `Device::createTimer` returns `Unimplemented`, preventing GPU timestamp/timer usage. | [Time Stamp Queries – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/time-stamp-queries) | Implement timers via `ID3D12QueryHeap` (TIMESTAMP) plus resolve buffers and expose them through the IGL timer API. | Open |
| DX12-FIND-12 | Medium | Texture Upload | `src/igl/d3d12/Texture.cpp:335-340` | `Texture::uploadCube` is a stub that returns `Unimplemented`, so cube textures cannot be populated. | [Texture Copy Operations – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations) | Implement cube uploads by iterating faces (six slices) and reusing the multi-subresource copy path already in `Texture::upload`. | Open |
| DX12-FIND-13 | Low | Draw Indirect | `src/igl/d3d12/RenderCommandEncoder.cpp:1158-1163` | `multiDrawIndirect` / `multiDrawIndexedIndirect` are empty, so IGL draw-indirect APIs cannot be used on D3D12. | [D3D12ExecuteIndirect Sample – Microsoft](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12ExecuteIndirect) | Implement these methods using `ExecuteIndirect` (or looped `Draw*Instanced`) and honor the IGL parameters. | Open |

## 3. Detailed Analyses (Analyst A–I & Chief Judge)
### Analyst-A – API & Initialization
- **DX12-FIND-01** shows the backend ignores `resourceBindingTier_` when constructing the graphics root signature; Tier‑1 requires bounded descriptor ranges yet the code always pushes unbounded tables. This breaks Microsoft’s compatibility guidance and prevents the backend from running on Tier‑1 laptops/WARP.
- Device capability validation (`Device::validateDeviceLimits`) only logs values; it never enforces caps nor propagates them to higher layers, so the rest of the stack assumes Vulkan-era limits and overruns DX12 resources.

### Analyst-B – Resources & Memory Lifetime
- **DX12-FIND-06** corrupts every RGBA texture by swapping bytes unnecessarily; the backend should honor DXGI channel order instead of massaging data.
- **DX12-FIND-07** forces a GPU flush per buffer upload which eliminates CPU/GPU overlap and regresses memory bandwidth. A per-frame staging allocator with fence retirement is required.
- **DX12-FIND-10** and **DX12-FIND-12** leave large parts of the texture/attachment API unusable (depth/stencil readback, cube uploads), blocking IGL scenarios already supported on Vulkan/OpenGL.

### Analyst-C – Descriptors & Root Signatures
- **DX12-FIND-02**, **DX12-FIND-05**, and **DX12-FIND-08** highlight systemic descriptor issues: no bounds checks, no headless resets, no storage buffer support, and a sampler heap artificially capped at 32 while Vulkan grows to hardware limits. These must be aligned with Microsoft’s descriptor heap best practices (MiniEngine/D3D12Hello* samples).
- **DX12-FIND-01** and **DX12-FIND-04** further show inconsistent root-signature usage between graphics and compute paths.

### Analyst-D – Command Queues & Synchronization
- **DX12-FIND-03** violates the `ID3D12CommandQueue::Signal` contract by reusing fence value `1`, so `waitUntilScheduled()` and any future timeline logic are incorrect after the first submission.
- Headless submissions never advance frame indices or reset descriptor counters (part of **DX12-FIND-02**), so long-running tests quickly overflow descriptors.
- The busy-wait + 5 s timeout path in `CommandQueue::submit` suggests the queue is already under stress; proper fence usage would remove the need for emergency “infinite wait”.

### Analyst-E – Shaders & Toolchain
- Push constants are capped at 64 bytes (**DX12-FIND-09**), half of what Vulkan exposes; shader authors using 96-128 bytes of push constants cannot port to DX12 even though the hardware supports it.
- `Device::createShaderModule` hardcodes DXC flags per build but never surfaces shader model, validator failures, or root signature compatibility checks; there is also no DXIL stripping for release builds.

### Analyst-F – Ray Tracing (DXR)
- The backend contains no DXR code paths (`rg` finds no `D3D12_RAYTRACING` identifiers), meaning DXR features exposed elsewhere in IGL cannot run on DX12. Microsoft’s DXR samples (DirectX-Graphics-Samples/Samples/Desktop/D3D12Raytracing) demonstrate required acceleration structures, which are entirely absent.

### Analyst-G – Performance & Scalability
- Forced GPU waits per upload (**DX12-FIND-07**), limited sampler heaps (**DX12-FIND-08**), and descriptor overflows (**DX12-FIND-02**) impose hard ceilings on throughput and scene complexity.
- Excessive logging (CommandQueue/CommandBuffer log every call) suggests the build is tuned for debugging; production builds should gate logs to avoid CPU overhead.

### Analyst-H – Robustness, Error Handling & Unimplemented Paths
- Multiple API surfaces return `Unimplemented` (Timers, cube uploads, depth/stencil copy, multi-draw) – see **DX12-FIND-10/11/12/13** and the stub audit below.
- Error handling around swaps/presents logs but then loops forever (busy waits) instead of surfacing a failure to the app.

### Analyst-I – Hard-coded Limits & Cross-API Validation
- Push constant size (**DX12-FIND-09**) and sampler heap size (**DX12-FIND-08**) diverge from Vulkan/GL backends, violating portability goals.
- Descriptor reset logic works only when a swapchain exists, whereas Vulkan/GLES manage descriptor pools per submission regardless of presentation.

### Chief-Judge – Integrated Verdict
The panel concludes the D3D12 backend is not production-ready. Critical descriptor/root signature bugs, broken synchronization, and large functional gaps (storage bind groups, cube uploads, timers, DXR) must be addressed before parity with the Vulkan/OpenGL backends can be claimed.

## 4. Cross-API Limit Audit
| Item | D3D12 Implementation | Vulkan / OpenGL Reference | Risk | Action |
|------|----------------------|--------------------------|------|--------|
| Sampler descriptors | Fixed `kMaxSamplers = 32`; per-frame sampler heaps are always 32 entries (Common.h:33; D3D12Context.cpp:528-541). | Vulkan dynamically grows bindless pools up to `maxDescriptorSetUpdateAfterBindSamplers` (src/igl/vulkan/VulkanContext.cpp:1125-1170). | ImGui + scene samplers easily exceed 32, causing crashes on DX12 but not on Vulkan/GL. | Resize sampler heaps based on `D3D12_FEATURE_DATA_D3D12_OPTIONS` and recycle descriptors per bind group. |
| Push constants | 16 DWORDs (64 bytes) reserved for compute and graphics (Device.cpp:736-747,932-943). | Vulkan pipeline state reserves 128 bytes (src/igl/vulkan/PipelineState.cpp:55-70). | Shaders using >64 bytes push constants compile for Vulkan but fail on DX12. | Raise the DX12 limit to the maximum allowed (64 DWORDs) and validate per shader stage. |
| Storage bind groups | Not implemented: storage buffers log “pending RS update” (RenderCommandEncoder.cpp:1518-1531). | Vulkan `ResourcesBinder` writes storage descriptors for bind groups (src/igl/vulkan/ResourcesBinder.cpp:52-185). | Any bind-group path with RW buffers works on Vulkan/GL but silently fails on DX12. | Implement SRV/UAV descriptor allocation and binding for storage slots. |
| Headless descriptor reset | Frame indices/descriptors are advanced only when a swapchain exists (CommandQueue.cpp:212-323). | Headless Vulkan contexts still recycle pools per submission. | Automated tests (headless) leak descriptors until the heap overflows. | Always rotate frame contexts and reset descriptor offsets regardless of swapchain presence. |

## 5. Compliance Matrix vs Official References
| Topic | Status | Evidence / Notes |
|-------|--------|------------------|
| Device creation & feature gating | Partial | Queries caps but ignores them (DX12-FIND-01); only logs root signature tier. |
| Descriptor heap management | No | Linear allocators without bounds or reset in headless mode (DX12-FIND-02). |
| Command queue synchronization | No | Fence values not monotonic; busy waits replace correct fence usage (DX12-FIND-03). |
| Resource upload path | Partial | Upload path exists but blocks via `waitForGPU` and lacks cube/depth features (DX12-FIND-06/07/10/12). |
| Shader/toolchain readiness | Partial | DXC integration exists but push constants limited and no DXR/advanced features (DX12-FIND-09, Analyst-F). |
| Profiling & telemetry | No | Timers unimplemented; no PIX markers beyond debug logs (DX12-FIND-11, Appendix E). |

## 6. Unimplemented & Stubbed Functions Audit
| ID | Severity | Symbol | Location | Stub Pattern | Callers Affected | Runtime Impact | Microsoft Ref | Suggested Impl / Plan | Owner | ETA |
|----|----------|--------|----------|--------------|------------------|----------------|----------------|------------------------|-------|-----|
| UA-01 | Medium | `Device::createTimer` | `src/igl/d3d12/Device.cpp:657` | Returns `Result::Code::Unimplemented` | Any consumer of IGL timers (perf HUD, telemetry) | No GPU timestamp queries available; profiling impossible. | [Time Stamp Queries – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/time-stamp-queries) | Implement query heaps (`D3D12_QUERY_HEAP_TYPE_TIMESTAMP`) plus resolve buffers and expose start/stop via the timer API. | DX12 | TBD |
| UA-02 | Medium | `Texture::uploadCube` | `src/igl/d3d12/Texture.cpp:335-340` | Returns `Result::Code::Unimplemented` | Cube map texture uploads/tests | Cube textures cannot be populated on DX12. | [Texture Copy Operations – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations) | Reuse the existing multi-subresource upload path to iterate six faces and copy via `CopyTextureRegion`. | DX12 | TBD |
| UA-03 | Medium | `Framebuffer::copyBytesDepthAttachment` | `src/igl/d3d12/Framebuffer.cpp:340-344` | `// TODO` | Readback paths needing depth data (tests, captures) | Depth buffer readbacks fail silently. | [Texture Copy Operations – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations) | Implement depth copy via temporary textures in `D3D12_RESOURCE_STATE_COPY_SOURCE` and map readback heaps. | DX12 | TBD |
| UA-04 | Medium | `Framebuffer::copyBytesStencilAttachment` | `src/igl/d3d12/Framebuffer.cpp:347-351` | `// TODO` | Same as UA-03 for stencil | Stencil readbacks unavailable. | [Texture Copy Operations – Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/texture-copy-operations) | Same approach as depth (copy to readback buffer, swizzle if needed). | DX12 | TBD |
| UA-05 | Low | `RenderCommandEncoder::multiDrawIndirect` / `multiDrawIndexedIndirect` | `src/igl/d3d12/RenderCommandEncoder.cpp:1158-1163` | Empty body | Any app using IGL multi-draw APIs | Features advertised by IGL do nothing on DX12. | [D3D12ExecuteIndirect Sample – Microsoft](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12ExecuteIndirect) | Implement via `ExecuteIndirect` or batched `Draw*Instanced`, respecting stride/count arguments. | DX12 | TBD |

## 7. Appendices
### Appendix A – Queue & Fence Timeline Diagram
```
Frame N:
  Record command list
  -> ExecuteCommandLists
  -> Signal scheduleFence (should be value++)
  -> (Optional) Present
  -> Signal frameFence (ctx.fenceValue++)
  -> Wait for frame N+1 resources
  -> Reset allocator + descriptor counters (currently only when swapchain != null)

Frame N+1:
  Uses frameContext[nextFrameIndex]
```
**Issues:** schedule fence never increments (DX12-FIND-03) and descriptor resets happen only when a swapchain exists (DX12-FIND-02).

### Appendix B – Resource-State Transition Map
| Operation | From State | To State | Source |
|-----------|------------|----------|--------|
| Begin render pass (offscreen MRT) | `PIXEL_SHADER_RESOURCE` | `RENDER_TARGET` | `RenderCommandEncoder.cpp:94-215` |
| End render pass (offscreen) | `RENDER_TARGET` | `PIXEL_SHADER_RESOURCE` | `RenderCommandEncoder.cpp:476-520` |
| Swapchain present | `RENDER_TARGET` | `PRESENT` | `RenderCommandEncoder.cpp:486-509` |
| Texture upload | `defaultState` | `COPY_DEST` -> `PIXEL_SHADER_RESOURCE` | `Texture.cpp:249-307` |
| Missing path | Depth/stencil copy uses TODOs (no transition) | `Framebuffer.cpp:340-347` |

### Appendix C – Descriptor-Heap Utilization Snapshot
| Heap (per frame) | Capacity | Allocation Strategy | Observed Issues |
|------------------|----------|---------------------|-----------------|
| CBV/SRV/UAV | 1024 descriptors | Linear bump pointer via `CommandBuffer::getNextCbvSrvUavDescriptor()` | No bounds check; headless mode never resets pointer (DX12-FIND-02). |
| Sampler | 32 descriptors | Linear bump pointer via `getNextSamplerDescriptor()` | Hard limit of 32 conflicts with spec limit 2048 (DX12-FIND-08). |
| RTV/DSV (DescriptorHeapManager) | 64 / 32 | Free-list | OK, but unused for shader-visible descriptors. |

### Appendix D – DXC Compile-Flag Matrix
| Build Mode | Flags | Source |
|------------|-------|--------|
| Debug builds | `-Zi -Qembed_debug -Od` plus strictness | `Device.cpp:1605-1634`, `DXCCompiler.cpp:64-137` |
| Release builds | `-O3` (still emits `-Zi` unless `IGL_D3D12_DISABLE_SHADER_DEBUG=1`) | Same as above |
| Optional toggles | `IGL_D3D12_SHADER_WARNINGS_AS_ERRORS=1` adds `-WX`; GPU-based validation via `IGL_D3D12_GPU_BASED_VALIDATION` | `Device.cpp:104-132`, `DXCCompiler.cpp:102-141` |

### Appendix E – PIX / ETW Profiling Notes
- Debug builds enable the D3D12 debug layer and DRED (DeviceRemovedExtendedData) (D3D12Context.cpp:120-188) but there are no PIX markers (`SetMarker`) or ETW guidance.
- GPU timers are unimplemented (DX12-FIND-11), so PIX captures cannot correlate GPU timing with engine events.

### Appendix F – Ray Tracing Status
- No DXR symbols or acceleration structure code exists under `src/igl/d3d12`. Contrast with Microsoft’s [D3D12Raytracing sample](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Raytracing); implementing DXR would require TLAS/BLAS builders, state objects, and shader table management.

### Appendix G – Unimplemented Coverage Map
| Priority | Stub (UA ID) | Dependencies | Notes |
|----------|--------------|--------------|-------|
| High | UA-01 | Requires query-heap management and fence retirement | Needed for profiling harness parity with Vulkan/GL. |
| High | UA-02 | Reuses existing upload command lists | Needed for environment parity (cube textures). |
| Medium | UA-03/04 | Shares staging path with color readbacks | Enables framebuffer readback tests/tools. |
| Low | UA-05 | Depends on descriptor fixes (DX12-FIND-02/05) | Implement once descriptor tracking is robust. |
