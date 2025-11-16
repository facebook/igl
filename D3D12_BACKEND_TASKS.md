## Task List

### T1 – Fix D3D12 descriptor/root binding and vertex buffer application

- **Priority:** High
- **Problem Summary:** Render and compute encoders create descriptors (SRVs/CBVs/UAVs) but fail to reliably bind them to the command list via `SetGraphicsRootDescriptorTable` / `SetComputeRootDescriptorTable`, and vertex buffers are cached without guaranteed `IASetVertexBuffers` before `Draw*`. This can result in textures and buffers not being visible to shaders or being bound with incorrect strides, a correctness issue unique to the D3D12 backend. Other backends bind descriptor sets/buffers explicitly at draw/dispatch.
- **Proposed Solution / Approach:**
  - Audit `RenderCommandEncoder` and `ComputeCommandEncoder` `bindTexture`, `bindBuffer`, and `draw*`/`dispatch` paths to ensure that for every bound resource, the root descriptor tables and root parameters are set before issuing any draw/dispatch.
  - Ensure `SetDescriptorHeaps` is called with the CBV/SRV/UAV and sampler heaps that contain the descriptors referenced in root tables at the beginning of encoding and whenever heaps change.
  - Correct the deferred vertex buffer binding logic so that `IASetVertexBuffers` is applied deterministically (either immediately in `bindVertexBuffer` or at a clearly-defined “apply bindings” point before all draw calls).
  - Align binding order and lifetime assumptions with the Vulkan/Metal resource binding abstractions so cross-backend behavior is consistent.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/RenderCommandEncoder.*`, `src/igl/d3d12/ComputeCommandEncoder.*`, `src/igl/d3d12/CommandBuffer.*`
  - Functions/Classes: `RenderCommandEncoder::bindTexture`, `RenderCommandEncoder::bindVertexBuffer`, `RenderCommandEncoder::draw*`, `ComputeCommandEncoder::bind*`, `ComputeCommandEncoder::dispatch`, `CommandBuffer` descriptor binding helpers.
- **Dependencies:** None
- **Microsoft Docs Reference:** `ID3D12GraphicsCommandList::SetDescriptorHeaps`, `SetGraphicsRootDescriptorTable`, `SetComputeRootDescriptorTable`, `IASetVertexBuffers` (“Descriptor heaps and views”, “Input-Assembler (IA) stage”).
- **Acceptance Criteria:**
  - All draw/dispatch paths set the appropriate descriptor heaps and root descriptor tables before issuing commands.
  - Vertex buffers (including stride and offset) are guaranteed to be applied before the first draw that uses them.
  - PIX captures show expected SRV/CBV/UAV bindings with no “unbound resource” warnings.
  - No regressions in Vulkan/Metal feature parity for resource binding behavior.

---

### T2 – Redesign GPU timer using D3D12 timestamp queries and fences

- **Priority:** High
- **Problem Summary:** The current `Timer` records timestamps and resolves query data without waiting for GPU completion; `resultsAvailable()` effectively returns true as soon as `end()` is called. This does not measure GPU execution time and diverges from Metal’s accurate GPU timers and Vulkan’s query/fence semantics.
- **Proposed Solution / Approach:**
  - Implement the canonical D3D12 timestamp pattern: `EndQuery` (start), record GPU work, `EndQuery` (end), `ResolveQueryData` into a readback buffer, and gate CPU reads on a queue fence reaching a known value.
  - Associate each timer instance with a fence value at which its query data is guaranteed resolved, and make `resultsAvailable()` check the fence’s `GetCompletedValue` against that value.
  - Expose an API similar to Metal’s `executionTimeNanos`/`getElapsedTimeNanos` where non-zero/valid durations are only returned after the fence has completed.
  - Ensure `CommandQueue::submit` integrates timer begin/end/resolve correctly around the actual GPU workload being measured, not just around command list closure.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Timer.*`, `src/igl/d3d12/CommandQueue.*`
  - Functions/Classes: `Timer::begin`, `Timer::end`, `Timer::resultsAvailable`, `Timer::getElapsedTimeNanos`, timer integration in `CommandQueue::submit`.
- **Dependencies:** None
- **Microsoft Docs Reference:** `ID3D12GraphicsCommandList::EndQuery`, `ResolveQueryData`, `ID3D12Fence`, and documentation for “Queries” and “Timestamp queries” in Direct3D 12.
- **Acceptance Criteria:**
  - Timer measures actual GPU execution intervals (verified via PIX or by comparing to known workloads).
  - `resultsAvailable()` only returns true once the associated fence value has completed.
  - No CPU reads of query result buffers occur before the corresponding fence has completed.
  - Metal and Vulkan backends report similar timing semantics for equivalent workloads.

---

### T3 – Replace exception-based error handling with Result-style flow

- **Priority:** High
- **Problem Summary:** The D3D12 backend throws `std::runtime_error` in `Device`, `D3D12Context`, `CommandQueue`, and related code paths, while other backends use Result-based error propagation and avoid exceptions. This divergence complicates cross-backend callers and can lead to unexpected termination patterns.
- **Proposed Solution / Approach:**
  - Identify all D3D12 code paths that throw exceptions for recoverable or reportable errors and replace them with Result-style return values or existing IGL error reporting mechanisms.
  - Align D3D12 error macros and helpers with Vulkan/Metal patterns (e.g., single log + assert, optional Result return) and avoid double-logging.
  - For truly fatal conditions (e.g., device lost with no recovery path), use consistent fatal handling (e.g., debug asserts or centralized crash path) instead of raw exceptions.
  - Ensure callers of D3D12 backend APIs have consistent error semantics with Vulkan/Metal (no surprises when switching backends).
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Device.*`, `src/igl/d3d12/D3D12Context.*`, `src/igl/d3d12/CommandQueue.*`, `src/igl/d3d12/Common.h`
  - Functions/Classes: Device construction/init helpers, `checkDeviceRemoval`, `validateDeviceLimits`, any `throw std::runtime_error` usage, `D3D12_CHECK`-style macros.
- **Dependencies:** None
- **Microsoft Docs Reference:** Not needed – solution already specified in report (API semantics do not require exceptions).
- **Acceptance Criteria:**
  - No remaining `throw` statements in D3D12 backend code paths that can be mapped to Result/logging.
  - Error reporting behavior (success/failure paths) matches Vulkan/Metal for equivalent scenarios.
  - Tests that previously passed continue to pass without uncaught exceptions.
  - Logging for failures is single, concise, and consistent across backends.

---

### T4 – Fix async upload race and standardize fence usage for uploads

- **Priority:** High
- **Problem Summary:** The current upload path can return buffers/textures before GPU uploads complete, while immediately transitioning resources to final states and exposing them to the rest of the system. This can cause race conditions where resources are used before data is valid. Vulkan handles this with staging buffers + fences; D3D12 must mirror this pattern.
- **Proposed Solution / Approach:**
  - For all asynchronous buffer/texture uploads, associate the operation with a fence value at which the upload is guaranteed complete, and ensure resources are not used (or at least documented as unsafe) before the fence signals.
  - Either block on the upload fence before returning from “synchronous” upload APIs, or modify API contracts and call sites to explicitly wait on a fence before using uploaded resources.
  - Centralize upload submission and fence management (even if UploadRingBuffer is retained) to avoid ad-hoc per-call `waitForGPU` usage and ensure consistent synchronization.
  - Align semantics and documentation with Vulkan’s upload APIs (e.g., staging + fence) so that shared higher-level code can rely on similar behavior.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Device.*`, `src/igl/d3d12/Buffer.*`, `src/igl/d3d12/Texture.*`, `src/igl/d3d12/UploadRingBuffer.*`, `src/igl/d3d12/CommandQueue.*`
  - Functions/Classes: Async upload helpers, `processCompletedUploads`, `Buffer::upload`, `Texture::uploadInternal`, `UploadRingBuffer::allocate`/submit paths.
- **Dependencies:** None
- **Microsoft Docs Reference:** `ID3D12Fence`, `ID3D12CommandQueue::Signal`, `SetEventOnCompletion`, “GPU synchronization with fences”.
- **Acceptance Criteria:**
  - No code paths expose uploaded resources as ready-to-use before corresponding GPU upload fences have signaled (unless explicitly documented).
  - Upload APIs either block until GPU completion or clearly expose fences/events that callers must respect.
  - Tests that stress uploads (large resources, repeated updates) behave correctly with no visual corruption.
  - The new upload synchronization pattern is clearly documented and consistent with Vulkan.

---

### T5 – Correct D3D12 state transition rules and UAV barrier usage

- **Priority:** High
- **Problem Summary:** `D3D12StateTransition` implements a custom “legal transitions” table that misclassifies some transitions (e.g., write-to-write transitions like `RENDER_TARGET` → `COPY_DEST`) as illegal, potentially inserting unnecessary barriers or blocking valid usage. Additionally, compute paths use a global UAV barrier (`pResource = nullptr`), which is overly conservative and can hurt performance.
- **Proposed Solution / Approach:**
  - Compare the existing transition rules against the D3D12 “Resource Barriers” documentation and remove any notion that write-to-write transitions are inherently illegal; in D3D12, applications may transition between any states, and the main constraint is that resources be in the correct state for each operation.
  - Simplify or remove `D3D12StateTransition`’s legal transitions table: either make it a minimal helper or replace with straightforward per-resource state tracking (single field) that always issues valid transitions.
  - Replace global UAV barriers in compute (`barrier.UAV.pResource = nullptr`) with resource-specific `pResource` barriers when a specific UAV is known, to limit synchronization scope and improve performance.
  - Remove unused validation/logging functions in `D3D12StateTransition` and ensure any remaining checks align strictly with documented illegal cases (e.g., `UNKNOWN` state, resource-type mismatches).
- **Scope / Impact:**
  - Files: `src/igl/d3d12/D3D12StateTransition.h`, `src/igl/d3d12/Buffer.*`, `src/igl/d3d12/Texture.*`, `src/igl/d3d12/ComputeCommandEncoder.*`
  - Functions/Classes: `validateTransition`, `getIntermediateState`, buffer/texture transition helpers, compute UAV barrier setup.
- **Dependencies:** None
- **Microsoft Docs Reference:** `D3D12_RESOURCE_BARRIER`, `D3D12_RESOURCE_BARRIER_TYPE_TRANSITION`, `D3D12_RESOURCE_BARRIER_TYPE_UAV`, `D3D12_RESOURCE_STATES`, and the “Resource barriers” section in Direct3D 12 docs.
- **Acceptance Criteria:**
  - Resource transitions no longer treat write-to-write transitions (e.g., `RENDER_TARGET` ↔ `COPY_DEST`) as illegal.
  - Global UAV barriers are replaced by resource-specific barriers where possible, with no functional regressions.
  - Unused logging/validation helpers in `D3D12StateTransition` are removed or significantly reduced.
  - PIX and debug-layer validation show no incorrect barrier usage while avoiding unnecessary barriers.

---

### T6 – Deduplicate major helper functions and shared utilities (~800 LOC)

- **Priority:** High
- **Problem Summary:** There are ~800+ lines of duplicated code across the D3D12 backend: large helpers like `executeCopyTextureToBuffer`, `generateMipmap`, readback routines, reflection lambdas, hash combine, feature-level stringification, and buffer descriptor creation appear multiple times with near-identical bodies. This increases maintenance cost and risk of divergence.
- **Proposed Solution / Approach:**
  - Extract shared helpers into central utilities:
    - Copy: create `TextureCopyHelper::copy()` used by CommandQueue and CommandBuffer instead of duplicate `executeCopyTextureToBuffer` implementations.
    - Mipmaps: create a single `Texture::generateMipmapInternal(cmdList, ...)` used by all mip generation entry points.
    - Readbacks: factor depth/stencil readback into `Framebuffer::readbackTexture(texture, plane, ...)`.
    - Reflection: extract `mapUniformType` and reflection parsing into a shared reflection utility used by both render and compute PSO creation.
    - Common helpers: move `hashCombine`, `featureLevelToString`, and `makeBufferDesc` into `Common.h` or a small `D3D12Utils` module.
  - Remove the duplicated implementations and adjust call sites to use the new shared functions.
  - Align naming and behavior with similar helpers in the Vulkan backend where appropriate.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/CommandQueue.*`, `src/igl/d3d12/CommandBuffer.*`, `src/igl/d3d12/Texture.*`, `src/igl/d3d12/Framebuffer.*`, `src/igl/d3d12/ShaderModule.*`, `src/igl/d3d12/RenderPipelineState.*`, `src/igl/d3d12/ComputePipelineState.*`, `src/igl/d3d12/Common.*`, `src/igl/d3d12/D3D12Context.*`, `src/igl/d3d12/Device.*`
  - Functions/Classes: `executeCopyTextureToBuffer`, `generateMipmap*`, depth/stencil readbacks, `mapUniformType`, `hashCombine`, `featureLevelToString`, buffer descriptor helpers.
- **Dependencies:** None
- **Microsoft Docs Reference:** Not needed – solution already specified in report.
- **Acceptance Criteria:**
  - All previously duplicated helper bodies exist in exactly one implementation each.
  - Callers in the listed files use the shared helpers with no behavior regressions.
  - Total duplicated LOC is reduced by ~800 as estimated in the review.
  - New utilities are clearly named and consistent with Vulkan backend analogs.

---

### T7 – Simplify and centralize upload/readback paths (immediate commands + staging)

- **Priority:** Medium
- **Problem Summary:** Upload and readback paths are over-engineered and fragmented: Buffer/Texture uploads, CommandBuffer `copyBuffer`, Framebuffer readback, UploadRingBuffer, and CommandQueue staging all implement their own allocators, command lists, fences, and waits. This leads to per-call `waitForGPU`, unnecessary resource recreation, and complexity far beyond Vulkan’s `VulkanImmediateCommands`/`VulkanStagingDevice` pattern.
- **Proposed Solution / Approach:**
  - Introduce a shared “immediate commands + staging” abstraction for D3D12 (e.g., `D3D12ImmediateCommands` + `D3D12StagingDevice`), modeled after Vulkan:
    - Single pool of command allocators and command lists for transient copies.
    - Shared upload and readback staging buffers (or UploadRingBuffer if retained).
    - Common fence/wait logic (reusing work from T4).
  - Refactor Buffer::upload, Texture::uploadInternal, `CommandBuffer::copyBuffer`, and Framebuffer readback to use this shared abstraction rather than ad-hoc self-contained upload paths.
  - Remove per-call `CreateCommandAllocator`/`waitForGPU` and ad-hoc readback caches unless a measurable performance benefit is demonstrated.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Buffer.*`, `src/igl/d3d12/Texture.*`, `src/igl/d3d12/Framebuffer.*`, `src/igl/d3d12/CommandBuffer.*`, `src/igl/d3d12/CommandQueue.*`, `src/igl/d3d12/UploadRingBuffer.*`
  - Functions/Classes: Upload/readback methods in Buffer/Texture/Framebuffer, `CommandBuffer::copyBuffer`, UploadRingBuffer, any per-call staging/fence logic.
- **Dependencies:** Depends on T4 (upload synchronization) and T6 (helper deduplication).
- **Microsoft Docs Reference:** `ID3D12CommandAllocator`, `ID3D12GraphicsCommandList`, `ID3D12Fence`, general “Command lists and command queues” docs.
- **Acceptance Criteria:**
  - All upload/readback flows share a common immediate-commands/staging abstraction.
  - No upload/readback path creates its own one-off command allocator/list and calls `waitForGPU` per operation.
  - Framebuffer readback cache complexity is either removed or reimplemented via the shared staging path with clear justification.
  - Throughput and latency match or improve on the current implementation in profiling.

---

### T8 – Introduce a D3D12 resource binder abstraction and unify descriptor management

- **Priority:** High
- **Problem Summary:** Descriptor management is fragmented: per-frame heaps, `DescriptorHeapManager`, and inline descriptor allocation in encoders coexist, with inconsistent lifetime, synchronization, and binding responsibilities. This deviates from Vulkan’s centralized `ResourcesBinder` pattern and makes it difficult to reason about descriptor lifetimes and bindings.
- **Proposed Solution / Approach:**
  - Design a `D3D12ResourcesBinder` (or similar) abstraction that:
    - Owns shader-visible CBV/SRV/UAV and sampler descriptor heaps per frame or per context.
    - Provides APIs for allocating descriptors, writing them, and binding root descriptor tables given pipeline layouts.
    - Encapsulates interaction with `DescriptorHeapManager` (or replaces it) and ensures heap growth/resizing policies are consistent and predictable.
  - Refactor `RenderCommandEncoder`, `ComputeCommandEncoder`, and `CommandBuffer` to use this binder for all descriptor allocation and binding instead of duplicating logic or mixing mechanisms.
  - Clearly separate headless/test descriptor heaps from frame-local heaps via configuration in the binder.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/D3D12Context.*`, `src/igl/d3d12/DescriptorHeapManager.*`, `src/igl/d3d12/CommandBuffer.*`, `src/igl/d3d12/RenderCommandEncoder.*`, `src/igl/d3d12/ComputeCommandEncoder.*`, potentially `src/igl/d3d12/Device.*`
  - Functions/Classes: `DescriptorHeapManager`, per-frame heap management in `FrameContext`, descriptor allocation/binding in encoders and command buffers.
- **Dependencies:** Depends on T1 (correct descriptor/root binding) and T6 (helper deduplication).
- **Microsoft Docs Reference:** `D3D12_DESCRIPTOR_HEAP_DESC`, `ID3D12Device::CreateDescriptorHeap`, `ID3D12Device::GetDescriptorHandleIncrementSize`, “Descriptor heaps and descriptors” documentation.
- **Acceptance Criteria:**
  - There is a single, well-defined D3D12 descriptor binding/binder abstraction used by encoders and command buffers.
  - Descriptor heap growth, allocation, and reuse policies are centralized and documented.
  - Previous overlapping descriptor systems (`DescriptorHeapManager` vs per-frame pages vs inline allocations) are either removed or clearly separated behind the binder.
  - Cross-backend behavior for “resource binding” is structurally similar to Vulkan’s `ResourcesBinder`.

---

### T9 – Refactor CommandQueue::submit and centralize fence/wait management

- **Priority:** High
- **Problem Summary:** `CommandQueue::submit` is ~453 lines long and handles submission, deferred copies, fences, present, frame advance, allocator reset, descriptor reset, and telemetry in a monolithic function. Fence waiting and event handling are implemented via verbose, defensive code that is hard to reason about and diverges from Vulkan/Metal patterns.
- **Proposed Solution / Approach:**
  - Split `submit` into focused subsystems:
    - `FrameManager` (frame index, allocator reset, descriptor heap reset),
    - `PresentManager` (swapchain handling),
    - `StagingManager` or “DeferredCopyManager” (deferred copy submission and fence coordination).
  - Introduce an RAII-style `FenceWaiter` to encapsulate event creation, timeout handling, and fence waiting; reuse it across submit and other wait sites.
  - Ensure deferred copies share fences/timelines with the main submission where appropriate, avoiding extra `waitForGPU` calls that stall CPU/GPU unnecessarily.
  - Target a clear, high-level `submit` function under ~100 lines that orchestrates helpers instead of implementing all logic inline.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/CommandQueue.*`, possibly helper headers for fence/RAII.
  - Functions/Classes: `CommandQueue::submit`, fence wait helpers, deferred copy handling.
- **Dependencies:** Depends on T4 (upload synchronization) and T7 (shared staging) for best effect.
- **Microsoft Docs Reference:** `ID3D12CommandQueue`, `ID3D12Fence`, `SetEventOnCompletion`, “Command lists and command queues”.
- **Acceptance Criteria:**
  - `CommandQueue::submit` is reduced from ~450 lines to a clearly structured function under ~100 lines using well-named helpers.
  - Fence waiting/event management logic is encapsulated in a reusable RAII helper.
  - Deferred copy submission no longer enforces unnecessary global waits that break “submit many, then wait” patterns.
  - Behavior matches or improves current correctness (including swapchain/present handling) with equivalent or better performance.

---

### T10 – Simplify buffer and texture state tracking to per-resource fields

- **Priority:** High
- **Problem Summary:** Buffer and texture state tracking is complex and sometimes incorrect: Buffer uses mutable state in const methods, Texture has a 275-line dual-mode tracking system, and `D3D12StateTransition` adds further complexity. Vulkan uses a single per-resource state field; Metal doesn’t track explicit states at all.
- **Proposed Solution / Approach:**
  - Replace mutable/const-violating state tracking with a simple, explicit per-resource state field (e.g., `currentState_`) that is updated only via non-const methods.
  - Remove the heterogeneous tracking mode in Texture and simplify to a single per-resource state model similar to Vulkan’s `imageLayout_`.
  - Integrate the simplified state tracking with the corrected barrier logic from T5.
  - Ensure the public API presents a clear contract for when state changes occur (e.g., uploads, copies, render passes).
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Buffer.*`, `src/igl/d3d12/Texture.*`, `src/igl/d3d12/D3D12StateTransition.h`
  - Functions/Classes: Buffer state fields and `upload` methods, Texture state tracking, transition helpers.
- **Dependencies:** Depends on T5 (state transition fix) and T15 (const-correctness) conceptually; can be implemented together.
- **Microsoft Docs Reference:** `D3D12_RESOURCE_STATES`, “Resource barriers” – conceptual model for resource states.
- **Acceptance Criteria:**
  - No buffer or texture methods mutate state through `mutable` members in `const` methods.
  - Texture state tracking is reduced from complex dual-mode logic to a simple per-resource state field.
  - All state transitions use the simplified state field plus barrier logic from T5.
  - Behavior matches Vulkan texture/buffer state semantics in higher-level IGL tests.

---

### T11 – Introduce a D3D12 pipeline builder and root signature generation from reflection

- **Priority:** Medium
- **Problem Summary:** Pipeline creation is a ~700-line monolithic function in `Device.cpp`, with duplicated reflection code, hard-coded root signatures, and verbose logging. This deviates from Vulkan’s `VulkanPipelineBuilder` and hinders maintenance and evolution of pipeline layouts.
- **Proposed Solution / Approach:**
  - Create a `D3D12PipelineBuilder` class that encapsulates pipeline creation (graphics and compute), similar in scope and responsibilities to the Vulkan pipeline builder.
  - Move shared reflection logic (`mapUniformType`, descriptor table layout derivation, root parameter generation) into a dedicated reflection utility and builder methods.
  - Generate root signatures from shader reflection (types, register spaces, visibility) instead of relying on hard-coded layouts; handle descriptors, root constants, and static samplers according to reflection output.
  - Reduce logging to debug-only or reflection-specific flags.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Device.*`, `src/igl/d3d12/ShaderModule.*`, `src/igl/d3d12/RenderPipelineState.*`, `src/igl/d3d12/ComputePipelineState.*`
  - Functions/Classes: Pipeline creation functions, reflection utilities, root signature creation.
- **Dependencies:** Depends on T6 (reflection deduplication) and benefits from T8 (resource binder abstraction).
- **Microsoft Docs Reference:** `D3D12_GRAPHICS_PIPELINE_STATE_DESC`, `D3D12_COMPUTE_PIPELINE_STATE_DESC`, `D3D12_ROOT_SIGNATURE_DESC`, `ID3D12Device::CreateGraphicsPipelineState`, `CreateRootSignature`, and the “Root signatures” documentation.
- **Acceptance Criteria:**
  - Pipeline creation logic is factored into a `D3D12PipelineBuilder` with clearly separated steps.
  - Reflection logic is shared between graphics and compute pipeline creation with no duplicated `mapUniformType` code.
  - Root signatures are built from reflection data rather than being hard-coded.
  - Pipeline creation code size is significantly reduced (from ~700 lines to ~100–150 across builder and helpers) with clear structure.

---

### T12 – Replace custom ComPtr implementation and pragma-based linking

- **Priority:** Medium
- **Problem Summary:** `D3D12Headers.h` reimplements `Microsoft::WRL::ComPtr` under the real namespace and uses `#pragma comment(lib, ...)` for library linking. The custom ComPtr is missing functionality (e.g., `As<>`, `ReleaseAndGetAddressOf`) and poses a maintenance risk; pragma-based linking is MSVC-specific and inconsistent with other backends’ build systems.
- **Proposed Solution / Approach:**
  - Replace the custom `Microsoft::WRL::ComPtr` implementation with the official WRL header (`<wrl/client.h>`), or, if necessary for some platforms, define a clearly-named local alias/wrapper type that cannot conflict with the real WRL namespace.
  - Update all usages to rely on the official ComPtr API, including `As<>`, `GetAddressOf`, `ReleaseAndGetAddressOf`, and comparison helpers where necessary.
  - Remove `#pragma comment(lib, ...)` from `D3D12Headers.h` and move library linkage into `CMakeLists.txt` using `target_link_libraries`.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/D3D12Headers.h`, top-level build files (`CMakeLists.txt` or equivalent).
  - Functions/Classes: Custom ComPtr type and any code that depends on its non-standard behavior.
- **Dependencies:** None
- **Microsoft Docs Reference:** `Microsoft::WRL::ComPtr` (WRL C++ template smart pointer docs).
- **Acceptance Criteria:**
  - No custom `Microsoft::WRL::ComPtr` implementation remains; official WRL or a clearly separated wrapper is used.
  - All code builds cleanly with the official ComPtr and uses its APIs where needed.
  - D3D12 library linking is handled by the build system rather than in headers.
  - No linker or ODR conflicts arise from including WRL in combination with existing code.

---

### T13 – Normalize error macros and drastically reduce logging volume

- **Priority:** Medium
- **Problem Summary:** The D3D12 backend logs errors multiple times (assert + error + INFO), has 10x–20x more INFO logging in hot paths than other backends, and scatters task-ID-style comments and verbose logs across almost every subsystem. This makes logs noisy and inconsistent with Vulkan/Metal.
- **Proposed Solution / Approach:**
  - Refine D3D12 error macros (`D3D12_CHECK`, validation macros) to perform a single log at error severity and an optional assert, mirroring Vulkan/Metal patterns.
  - Introduce or standardize debug macros like `IGL_D3D12_PRINT_COMMANDS` and `IGL_D3D12_DEBUG_VERBOSE` and gate non-critical INFO/DEBUG logs behind them.
  - Remove destructor and hot-path logging (e.g., per-object destruction, per-dispatch, per-draw logs) unless they’re explicitly controlled by debug macros.
  - Remove task-ID comments and AI-style verbose commentary, keeping only concise, behavior-focused comments aligned with other backends.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Common.h`, `src/igl/d3d12/Device.*`, `src/igl/d3d12/D3D12Context.*`, `src/igl/d3d12/CommandQueue.*`, `src/igl/d3d12/CommandBuffer.*`, `src/igl/d3d12/RenderCommandEncoder.*`, `src/igl/d3d12/ComputeCommandEncoder.*`, `src/igl/d3d12/Framebuffer.*`, `src/igl/d3d12/Texture.*`, etc.
  - Functions/Classes: Logging and validation helpers, destructor logs, per-call INFO logs, comments across the backend.
- **Dependencies:** None (complements T3).
- **Microsoft Docs Reference:** Not needed – logging behavior is library policy.
- **Acceptance Criteria:**
  - Logging call count in D3D12 backend is within the same order of magnitude as Vulkan (target ~25–50 calls in core paths).
  - No double-logging (assert + error) patterns remain for a single failure.
  - Hot-path destructor and per-draw/dispatch INFO logs are either removed or behind debug macros.
  - Task ID and AI-style comments are replaced with concise, backend-consistent comments.

---

### T14 – Remove dead and unnecessary D3D12 utilities and code

- **Priority:** Medium
- **Problem Summary:** Several utilities and code paths are effectively dead or unjustified: `ResourceAlignment.h` is mostly unused and speculative, `SamplerState.cpp` is empty, high-watermark tracking is never read, `UploadRingBuffer::canAllocate` is unused, D3D12StateTransition logging helpers have zero callers, etc. These add cognitive load without benefits.
- **Proposed Solution / Approach:**
  - Delete `ResourceAlignment.h` entirely or reduce it to a minimal `AlignUp<>` template and necessary constants; remove unused placed-resource helpers and derived constants that duplicate SDK names.
  - Remove `SamplerState.cpp` if it has no implementation, or move any needed symbols into the header.
  - Delete unused tracking and metrics such as high-watermark counters and unused logging utilities (`validateTransition`, state-name logging switches) where they have no observable effect.
  - Remove unused helper methods like `UploadRingBuffer::canAllocate` and unreachable “race-avoidance” code given existing mutex usage.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/ResourceAlignment.h`, `src/igl/d3d12/SamplerState.*`, `src/igl/d3d12/DescriptorHeapManager.*`, `src/igl/d3d12/UploadRingBuffer.*`, `src/igl/d3d12/D3D12StateTransition.h`, others as identified.
  - Functions/Classes: Dead logging functions, unused helpers, speculative placed-resource utilities.
- **Dependencies:** None
- **Microsoft Docs Reference:** Not needed – removing unused code; use SDK constants where necessary.
- **Acceptance Criteria:**
  - All identified dead code paths (unused functions, metrics, empty implementations) are removed.
  - `ResourceAlignment.h` is either deleted or contains only actively used helpers and uses SDK-defined constants.
  - Build and tests pass with no references to removed symbols.
  - Overall D3D12 backend code size and complexity are measurably reduced.

---

### T15 – Fix const-correctness violations and static mutable state

- **Priority:** High
- **Problem Summary:** Multiple classes mark methods as `const` while mutating state via `mutable` members (e.g., Buffer state, Device upload queues), and static mutable state (e.g., DXC compiler initialization in Texture, static call counters) is used without proper synchronization. This undermines thread safety and clarity.
- **Proposed Solution / Approach:**
  - Audit all `const` methods that modify internal state (via `mutable` or other means) and either:
    - Remove `const` and treat them as mutating operations, or
    - Refactor to avoid state mutation inside `const` methods (e.g., move caching or lazy initialization elsewhere).
  - Remove static call counters and similar constructs used only for “log first N times” patterns; they are not thread-safe and add little value.
  - Move static DXC compiler initialization and similar singleton-like state into device initialization or guard with `std::call_once` and a properly synchronized static.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Buffer.*`, `src/igl/d3d12/Device.*`, `src/igl/d3d12/Texture.*`, possibly others.
  - Functions/Classes: Buffer and Device methods that are `const` but mutate state, DXC initialization blocks, static counters.
- **Dependencies:** None (but complements T10).
- **Microsoft Docs Reference:** Not needed – C++ const-correctness and threading discipline.
- **Acceptance Criteria:**
  - No methods marked `const` mutate internal state or rely on `mutable` for non-trivial logic.
  - DXC compiler or other static initializations are thread-safe (`std::call_once` or explicit device lifetime control).
  - Static call counters used only for logging are removed.
  - Thread-safety analysis of async paths no longer flags obvious shared mutable state races.

---

### T16 – Replace hard-coded sizes/alignments with D3D12 constants and configuration

- **Priority:** Medium
- **Problem Summary:** The backend uses many hard-coded values for descriptor heap sizes, upload buffer sizes (e.g., 128MB), frame count (e.g., `kMaxFramesInFlight = 3`), and alignment (e.g., `kUploadAlignment = 256`) without consistently using D3D12 named constants or device queries. This causes portability and tuning issues, especially on mobile or memory-constrained platforms.
- **Proposed Solution / Approach:**
  - Replace hard-coded alignment and placement values with D3D12 constants such as:
    - `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`,
    - `D3D12_TEXTURE_DATA_PITCH_ALIGNMENT`,
    - `D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT` as appropriate.
  - Move descriptor heap sizes and upload buffer defaults into a D3D12 context configuration struct, and where possible query device limits (e.g., via `CheckFeatureSupport`) or allow user configuration.
  - Review and adjust fixed-size choices (heap sizes, upload ring size, frames in flight) to either be configurable or at least documented with rationale consistent across backends.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/Common.h`, `src/igl/d3d12/Buffer.*`, `src/igl/d3d12/Texture.*`, `src/igl/d3d12/DescriptorHeapManager.*`, `src/igl/d3d12/UploadRingBuffer.*`, `src/igl/d3d12/CommandQueue.*`, `src/igl/d3d12/ResourceAlignment.h`
  - Functions/Classes: Constants for heap sizes and alignments, upload ring size defaults, frame count constants.
- **Dependencies:** None
- **Microsoft Docs Reference:** `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`, `D3D12_TEXTURE_DATA_PITCH_ALIGNMENT`, `D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT`, `ID3D12Device::CheckFeatureSupport`.
- **Acceptance Criteria:**
  - All alignment-sensitive code uses D3D12-defined constants rather than magic numbers where applicable.
  - Descriptor heap sizes, upload buffer sizes, and frames in flight are configurable via a context/config struct and/or derived from device capabilities.
  - Rationale for any remaining fixed values is documented and consistent across backends.
  - No hard-coded values remain in D3D12 backend that conflict with documented D3D12 alignment or placement requirements.

---

### T17 – Re-evaluate UploadRingBuffer and descriptor heap manager complexity

- **Priority:** Medium
- **Problem Summary:** `UploadRingBuffer` and `DescriptorHeapManager` add significant complexity (multiple failure modes, atomics under mutex, redundant tracking) without clear evidence of performance benefit over simpler approaches. The review questions whether these systems are necessary given D3D12 upload heaps and simpler descriptor management patterns.
- **Proposed Solution / Approach:**
  - Simplify `UploadRingBuffer` by:
    - Removing unnecessary atomics where a mutex already serializes access.
    - Fixing the empty/non-empty representation (`tail_ == head_` for empty) and removing unreachable “wraparound race” code.
    - Adding chunked upload support (`allocateChunked`) if large (>128MB) uploads are required.
  - Measure performance against a simple “allocate on-demand upload buffer + fence” path; if no meaningful gain is observed for target workloads, deprecate and remove UploadRingBuffer in favor of the simpler approach.
  - Simplify `DescriptorHeapManager` by removing redundant APIs (keep one `getXHandle` variant) and redundant tracking (free lists vs bitmaps), and reducing per-access locking costs where safe (e.g., using atomics or per-page structures).
- **Scope / Impact:**
  - Files: `src/igl/d3d12/UploadRingBuffer.*`, `src/igl/d3d12/DescriptorHeapManager.*`
  - Functions/Classes: Ring buffer head/tail logic, `allocate`/`allocateChunked`, `canAllocate`, descriptor handle allocation and tracking.
- **Dependencies:** Depends on T7 (shared staging) and T8 (binder abstraction) for the final shape of upload/descriptor management.
- **Microsoft Docs Reference:** D3D12 upload heap semantics (`D3D12_HEAP_TYPE_UPLOAD`), descriptor heap usage docs provide context; main changes are architectural.
- **Acceptance Criteria:**
  - UploadRingBuffer has no unnecessary atomics or unreachable race-avoidance code, and supports chunked uploads if needed.
  - A baseline simple upload path exists and is benchmarked against UploadRingBuffer; if no benefit is found, UploadRingBuffer is removed from production paths.
  - DescriptorHeapManager uses a single coherent API for handle allocation and minimal redundant tracking.
  - Complexity of these utilities is reduced while preserving or improving performance.

---

### T18 – Clean up SamplerState and enable sampler caching

- **Priority:** Low
- **Problem Summary:** `SamplerState.cpp` is effectively empty, and samplers lack hash/equality functions to support caching. Metal and Vulkan backends maintain sampler caches keyed by sampler state; D3D12 should mirror this pattern for consistency and performance.
- **Proposed Solution / Approach:**
  - Remove `SamplerState.cpp` if it only contains an empty namespace; move any needed symbols into the header.
  - Implement `hash()` and equality for sampler descriptors so that D3D12 `Device` or a dedicated sampler cache can reuse matching samplers.
  - Align sampler caching strategy with Vulkan/Metal: a small map keyed by sampler parameters to existing D3D12 sampler descriptors or sampler objects.
- **Scope / Impact:**
  - Files: `src/igl/d3d12/SamplerState.*`, `src/igl/d3d12/Device.*`
  - Functions/Classes: `SamplerState` class, sampler cache in Device or new helper.
- **Dependencies:** None
- **Microsoft Docs Reference:** `D3D12_SAMPLER_DESC`, “Samplers” documentation (for field semantics); caching strategy is internal.
- **Acceptance Criteria:**
  - `SamplerState` has proper hash/equality support.
  - Sampler caching is implemented and used in Device where samplers are created.
  - `SamplerState.cpp` is removed or contains meaningful implementation.
  - Behavior matches Vulkan/Metal sampler caching semantics (reusing identical sampler states).

